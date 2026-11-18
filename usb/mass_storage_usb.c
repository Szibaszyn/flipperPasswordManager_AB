#include "mass_storage_usb.h"
#include <furi_hal.h>
#include <furi.h>

#define TAG "MassStorageUsb"

#define USB_MSC_RX_EP       0x01
#define USB_MSC_TX_EP       0x82
#define USB_MSC_EP_SIZE     64UL

#define USB_MSC_GET_MAX_LUN 0xFE
#define USB_MSC_BOT_RESET   0xFF

#define CBW_SIGNATURE 0x43425355
#define CSW_SIGNATURE 0x53425355

#define USB_MSC_BUF_MAX (0x10000UL - SCSI_BLOCK_SIZE)

typedef enum {
    EventExit  = 1 << 0,
    EventReset = 1 << 1,
    EventRxTx  = 1 << 2,
    EventAll   = EventExit | EventReset | EventRxTx,
} MassStorageEvent;

typedef struct {
    uint32_t sig;
    uint32_t tag;
    uint32_t len;
    uint8_t flags;
    uint8_t lun;
    uint8_t cmd_len;
    uint8_t cmd[16];
} __attribute__((packed)) CBW;

typedef struct {
    uint32_t sig;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
} __attribute__((packed)) CSW;

struct MassStorageUsb {
    FuriHalUsbInterface usb;
    FuriHalUsbInterface* usb_prev;
    FuriThread* thread;
    usbd_device* dev;
    SCSIDeviceFunc fn;
    bool connected;
    MassStorageUsbConnectionStatusCallback status_cb;
    void* status_cb_context;
};

static usbd_respond msc_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond msc_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

static int32_t mass_thread_worker(void* context) {
    MassStorageUsb* mass = context;
    usbd_device* dev = mass->dev;
    SCSISession scsi = {.fn = mass->fn};
    CBW cbw;
    CSW csw;
    memset(&cbw, 0, sizeof(cbw));
    memset(&csw, 0, sizeof(csw));

    uint8_t* buf = NULL;
    uint32_t buf_len = 0, buf_cap = 0, buf_sent = 0;

    enum {
        StateReadCBW,
        StateReadData,
        StateWriteData,
        StateBuildCSW,
        StateWriteCSW,
    } state = StateReadCBW;

    while(true) {
        uint32_t flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & EventExit) break;

        if(flags & EventReset) {
            scsi.sk = 0;
            scsi.asc = 0;
            memset(&cbw, 0, sizeof(cbw));
            memset(&csw, 0, sizeof(csw));
            if(buf) { free(buf); buf = NULL; }
            buf_len = buf_cap = buf_sent = 0;
            state = StateReadCBW;
        }

        if(flags & EventRxTx) do {
            switch(state) {
            case StateReadCBW: {
                int32_t len = usbd_ep_read(dev, USB_MSC_RX_EP, &cbw, sizeof(cbw));
                if(len <= 0) break;

                if(!mass->connected) {
                    mass->connected = true;
                    if(mass->status_cb) mass->status_cb(true, mass->status_cb_context);
                }

                if(len != sizeof(cbw) || cbw.sig != CBW_SIGNATURE) {
                    usbd_ep_stall(dev, USB_MSC_TX_EP);
                    usbd_ep_stall(dev, USB_MSC_RX_EP);
                    continue;
                }

                if(!scsi_cmd_start(&scsi, cbw.cmd, cbw.cmd_len)) {
                    usbd_ep_stall(dev, USB_MSC_RX_EP);
                    csw.sig = CSW_SIGNATURE;
                    csw.tag = cbw.tag;
                    csw.status = 1;
                    state = StateWriteCSW;
                    continue;
                }

                if(cbw.flags & 0x80) {
                    buf_len = 0;
                    buf_sent = 0;
                    state = StateWriteData;
                } else {
                    buf_len = 0;
                    state = StateReadData;
                }
                continue;
            }
            case StateReadData: {
                if(!cbw.len) { state = StateBuildCSW; continue; }

                uint32_t buf_clamp = (cbw.len < USB_MSC_BUF_MAX) ? cbw.len : USB_MSC_BUF_MAX;
                if(buf_clamp > buf_cap) {
                    if(buf) free(buf);
                    buf_cap = buf_clamp;
                    buf = malloc(buf_cap);
                }

                if(buf_len < buf_clamp) {
                    int32_t len = usbd_ep_read(dev, USB_MSC_RX_EP, buf + buf_len, buf_clamp - buf_len);
                    if(len < 0) break;
                    buf_len += len;
                }

                if(buf_len == buf_clamp) {
                    if(!scsi_cmd_rx_data(&scsi, buf, buf_len)) {
                        usbd_ep_stall(dev, USB_MSC_RX_EP);
                        csw.sig = CSW_SIGNATURE;
                        csw.tag = cbw.tag;
                        csw.status = 1;
                        csw.residue = cbw.len;
                        state = StateWriteCSW;
                        continue;
                    }
                    cbw.len -= buf_len;
                    buf_len = 0;
                }
                continue;
            }
            case StateWriteData: {
                if(!cbw.len) { state = StateBuildCSW; continue; }

                uint32_t buf_clamp = (cbw.len < USB_MSC_BUF_MAX) ? cbw.len : USB_MSC_BUF_MAX;
                if(buf_clamp > buf_cap) {
                    if(buf) free(buf);
                    buf_cap = buf_clamp;
                    buf = malloc(buf_cap);
                }

                if(!buf_len && !scsi_cmd_tx_data(&scsi, buf, &buf_len, buf_clamp)) {
                    state = StateBuildCSW;
                    continue;
                }

                uint32_t chunk = (buf_len - buf_sent < USB_MSC_EP_SIZE) ? (buf_len - buf_sent) : USB_MSC_EP_SIZE;
                int32_t len = usbd_ep_write(dev, USB_MSC_TX_EP, buf + buf_sent, chunk);
                if(len < 0) break;

                buf_sent += len;
                if(buf_sent == buf_len) {
                    cbw.len -= buf_len;
                    buf_len = 0;
                    buf_sent = 0;
                }
                continue;
            }
            case StateBuildCSW: {
                csw.sig = CSW_SIGNATURE;
                csw.tag = cbw.tag;
                csw.status = scsi_cmd_end(&scsi) ? 0 : 1;
                csw.residue = cbw.len;
                state = StateWriteCSW;
                continue;
            }
            case StateWriteCSW: {
                int32_t len = usbd_ep_write(dev, USB_MSC_TX_EP, &csw, sizeof(csw));
                if(len < 0) break;

                memset(&cbw, 0, sizeof(cbw));
                memset(&csw, 0, sizeof(csw));
                state = StateReadCBW;
                continue;
            }
            }
            break;
        } while(true);
    }

    if(buf) free(buf);
    return 0;
}

static MassStorageUsb* g_mass_inst = NULL;

static void usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    MassStorageUsb* mass = ctx;
    g_mass_inst = mass;
    mass->dev = dev;
    mass->connected = false;

    usbd_reg_config(dev, msc_ep_config);
    usbd_reg_control(dev, msc_control);
    usbd_connect(dev, true);

    furi_thread_start(mass->thread);
}

static void usb_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    MassStorageUsb* mass = g_mass_inst;
    if(!mass || mass->dev != dev) return;
    g_mass_inst = NULL;

    if(mass->thread) {
        furi_thread_flags_set(furi_thread_get_id(mass->thread), EventExit);
        furi_thread_join(mass->thread);
    }
}

static void usb_wakeup(usbd_device* dev) { UNUSED(dev); }

static void usb_suspend(usbd_device* dev) {
    MassStorageUsb* mass = g_mass_inst;
    if(mass && mass->dev == dev && mass->thread) {
        furi_thread_flags_set(furi_thread_get_id(mass->thread), EventReset);
    }
}

static void usb_rxtx_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep); UNUSED(event);
    MassStorageUsb* mass = g_mass_inst;
    if(mass && mass->dev == dev && mass->thread) {
        furi_thread_flags_set(furi_thread_get_id(mass->thread), EventRxTx);
    }
}

static usbd_respond msc_ep_config(usbd_device* dev, uint8_t cfg) {
    if(cfg == 0) {
        usbd_ep_deconfig(dev, USB_MSC_RX_EP);
        usbd_ep_deconfig(dev, USB_MSC_TX_EP);
        usbd_reg_endpoint(dev, USB_MSC_RX_EP, NULL);
        usbd_reg_endpoint(dev, USB_MSC_TX_EP, NULL);
        return usbd_ack;
    } else if(cfg == 1) {
        usbd_ep_config(dev, USB_MSC_RX_EP, USB_EPTYPE_BULK, USB_MSC_EP_SIZE);
        usbd_ep_config(dev, USB_MSC_TX_EP, USB_EPTYPE_BULK, USB_MSC_EP_SIZE);
        usbd_reg_endpoint(dev, USB_MSC_RX_EP, usb_rxtx_callback);
        usbd_reg_endpoint(dev, USB_MSC_TX_EP, usb_rxtx_callback);
        return usbd_ack;
    }
    return usbd_fail;
}

static usbd_respond msc_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) !=
       (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
        return usbd_fail;
    }
    if(req->bRequest == USB_MSC_GET_MAX_LUN) {
        static uint8_t max_lun = 0;
        dev->status.data_ptr = &max_lun;
        dev->status.data_count = 1;
        return usbd_ack;
    } else if(req->bRequest == USB_MSC_BOT_RESET) {
        MassStorageUsb* mass = g_mass_inst;
        if(mass && mass->dev == dev && mass->thread) {
            furi_thread_flags_set(furi_thread_get_id(mass->thread), EventReset);
        }
        return usbd_ack;
    }
    return usbd_fail;
}

static const struct usb_string_descriptor msc_manuf_desc = USB_STRING_DESC("Flipper Devices Inc.");

struct MscConfigDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor ep_rx;
    struct usb_endpoint_descriptor ep_tx;
} __attribute__((packed));

static const struct usb_device_descriptor msc_dev_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 8,
    .idVendor = 0x0483,
    .idProduct = 0x5720,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct MscConfigDescriptor msc_cfg_desc = {
    .config = {
        .bLength = sizeof(struct usb_config_descriptor),
        .bDescriptorType = USB_DTYPE_CONFIGURATION,
        .wTotalLength = sizeof(struct MscConfigDescriptor),
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = NO_DESCRIPTOR,
        .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
        .bMaxPower = USB_CFG_POWER_MA(100),
    },
    .intf = {
        .bLength = sizeof(struct usb_interface_descriptor),
        .bDescriptorType = USB_DTYPE_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_MASS_STORAGE,
        .bInterfaceSubClass = 0x06, // SCSI transparent
        .bInterfaceProtocol = 0x50, // Bulk-only transport
        .iInterface = NO_DESCRIPTOR,
    },
    .ep_rx = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = USB_MSC_RX_EP,
        .bmAttributes = USB_EPTYPE_BULK,
        .wMaxPacketSize = USB_MSC_EP_SIZE,
        .bInterval = 0,
    },
    .ep_tx = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = USB_MSC_TX_EP,
        .bmAttributes = USB_EPTYPE_BULK,
        .wMaxPacketSize = USB_MSC_EP_SIZE,
        .bInterval = 0,
    },
};

MassStorageUsb* mass_storage_usb_start(const char* filename, SCSIDeviceFunc fn) {
    MassStorageUsb* mass = malloc(sizeof(MassStorageUsb));
    memset(mass, 0, sizeof(MassStorageUsb));

    mass->usb_prev = furi_hal_usb_get_config();
    mass->usb.init = usb_init;
    mass->usb.deinit = usb_deinit;
    mass->usb.wakeup = usb_wakeup;
    mass->usb.suspend = usb_suspend;
    mass->usb.dev_descr = (struct usb_device_descriptor*)&msc_dev_desc;
    mass->usb.str_manuf_descr = (void*)&msc_manuf_desc;
    mass->usb.cfg_descr = (void*)&msc_cfg_desc;

    const char* dev_name = furi_hal_version_get_device_name_ptr();
    if(!dev_name) dev_name = "Flipper Zero";
    size_t dev_len = strlen(dev_name);
    struct usb_string_descriptor* str_prod = malloc(dev_len * 2 + 2);
    str_prod->bLength = dev_len * 2 + 2;
    str_prod->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < dev_len; i++) str_prod->wString[i] = dev_name[i];
    mass->usb.str_prod_descr = str_prod;

    size_t fn_len = strlen(filename);
    struct usb_string_descriptor* str_serial = malloc(fn_len * 2 + 2);
    str_serial->bLength = fn_len * 2 + 2;
    str_serial->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < fn_len; i++) str_serial->wString[i] = filename[i];
    mass->usb.str_serial_descr = str_serial;

    mass->fn = fn;
    mass->thread = furi_thread_alloc_ex("MscUsbTh", 4096, mass_thread_worker, mass);

    if(!furi_hal_usb_set_config(&mass->usb, mass)) {
        furi_thread_free(mass->thread);
        free(mass->usb.str_prod_descr);
        free(mass->usb.str_serial_descr);
        free(mass);
        return NULL;
    }
    return mass;
}

void mass_storage_usb_stop(MassStorageUsb* mass) {
    if(!mass) return;
    furi_hal_usb_set_config(mass->usb_prev, NULL);
    furi_thread_free(mass->thread);
    free(mass->usb.str_prod_descr);
    free(mass->usb.str_serial_descr);
    free(mass);
}

void mass_storage_usb_set_connection_status_callback(
    MassStorageUsb* mass,
    MassStorageUsbConnectionStatusCallback cb,
    void* context) {
    if(mass) {
        mass->status_cb = cb;
        mass->status_cb_context = context;
    }
}
