#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"
#include "../fat12/fat12_disk.h"
#include <notification/notification_messages.h>

#define TAG "SceneSaveMode"

static bool msc_disk_read(
    void* ctx,
    uint32_t lba,
    uint16_t count,
    uint8_t* out,
    uint32_t* out_len,
    uint32_t out_cap) {
    FlipperPwmApp* app = ctx;
    if(!storage_file_seek(app->disk_file, lba * SCSI_BLOCK_SIZE, true)) {
        return false;
    }
    uint32_t needed = count * SCSI_BLOCK_SIZE;
    uint32_t to_read = (needed < out_cap) ? needed : out_cap;
    uint32_t rem = to_read;
    uint8_t* p = out;
    *out_len = 0;
    while(rem > 0) {
        uint16_t r = storage_file_read(app->disk_file, p, rem);
        if(r == 0) return false;
        p += r;
        rem -= r;
        *out_len += r;
    }
    return true;
}

static bool msc_disk_write(void* ctx, uint32_t lba, uint16_t count, uint8_t* buf, uint32_t len) {
    FlipperPwmApp* app = ctx;
    if(len != (uint32_t)count * SCSI_BLOCK_SIZE) return false;
    if(!storage_file_seek(app->disk_file, lba * SCSI_BLOCK_SIZE, true)) return false;
    uint32_t rem = len;
    uint8_t* p = buf;
    while(rem > 0) {
        uint16_t w = storage_file_write(app->disk_file, p, rem);
        if(w == 0) return false;
        p += w;
        rem -= w;
    }
    return true;
}

static uint32_t msc_disk_num_blocks(void* ctx) {
    FlipperPwmApp* app = ctx;
    return storage_file_size(app->disk_file) / SCSI_BLOCK_SIZE;
}

static void msc_disk_eject(void* ctx) {
    FlipperPwmApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, PwmEventSaveModeDone);
}

static void widget_save_btn_callback(GuiButtonType result, InputType type, void* context) {
    FlipperPwmApp* app = context;
    if(type == InputTypeShort) {
        if(result == GuiButtonTypeCenter) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PwmEventSaveModeDone);
        } else if(result == GuiButtonTypeLeft) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PwmEventSaveModeCancel);
        }
    }
}

void flipper_pwm_scene_save_mode_on_enter(void* context) {
    FlipperPwmApp* app = context;

    storage_simply_mkdir(app->storage, PWM_APP_DATA_DIR);

    // Format / write current vault data to 64KB disk image
    fat12_disk_create(PWM_DISK_IMG_PATH, &app->vault, app->storage);

    // Open disk image file for USB MSC
    app->disk_file = storage_file_alloc(app->storage);
    if(!storage_file_open(app->disk_file, PWM_DISK_IMG_PATH, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Cannot open disk image file for USB");
        storage_file_free(app->disk_file);
        app->disk_file = NULL;
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    SCSIDeviceFunc fn = {
        .ctx = app,
        .read = msc_disk_read,
        .write = msc_disk_write,
        .num_blocks = msc_disk_num_blocks,
        .eject = msc_disk_eject,
    };

    app->usb_msc = mass_storage_usb_start("passwords.img", fn);

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "USB Drive (64KB)");
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary, "1. Edit passwords.txt");
    widget_add_string_element(
        app->widget, 64, 27, AlignCenter, AlignTop, FontSecondary, "Format: pass:@:name");
    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignTop, FontSecondary, "2. Press Save when done");

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Save", widget_save_btn_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", widget_save_btn_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewWidget);
}

bool flipper_pwm_scene_save_mode_on_event(void* context, SceneManagerEvent event) {
    FlipperPwmApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PwmEventSaveModeDone) {
            // Stop USB MSC
            if(app->usb_msc) {
                mass_storage_usb_stop(app->usb_msc);
                app->usb_msc = NULL;
            }
            if(app->disk_file) {
                storage_file_close(app->disk_file);
                storage_file_free(app->disk_file);
                app->disk_file = NULL;
            }

            // Read passwords.txt from the FAT12 disk image
            fat12_disk_read_vault(PWM_DISK_IMG_PATH, &app->vault, app->storage);

            // Save vault directly to SD card
            vault_save_file(app->storage, PWM_VAULT_FILE_PATH, &app->vault, NULL);

            // Buzz success notification
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_success);
            furi_record_close(RECORD_NOTIFICATION);

            // Show success widget
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "Vault Saved!");
            char msg[48];
            snprintf(msg, sizeof(msg), "Stored %u passwords", (unsigned int)app->vault.entry_count);
            widget_add_string_element(
                app->widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, msg);
            widget_add_string_element(
                app->widget, 64, 46, AlignCenter, AlignCenter, FontSecondary, "Press Back to return");

            view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewWidget);
            consumed = true;
        } else if(event.event == PwmEventSaveModeCancel) {
            if(app->usb_msc) {
                mass_storage_usb_stop(app->usb_msc);
                app->usb_msc = NULL;
            }
            if(app->disk_file) {
                storage_file_close(app->disk_file);
                storage_file_free(app->disk_file);
                app->disk_file = NULL;
            }
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipperPwmSceneMainMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(!app->usb_msc) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipperPwmSceneMainMenu);
            consumed = true;
        }
    }

    return consumed;
}

void flipper_pwm_scene_save_mode_on_exit(void* context) {
    FlipperPwmApp* app = context;
    if(app->usb_msc) {
        mass_storage_usb_stop(app->usb_msc);
        app->usb_msc = NULL;
    }
    if(app->disk_file) {
        storage_file_close(app->disk_file);
        storage_file_free(app->disk_file);
        app->disk_file = NULL;
    }
    widget_reset(app->widget);
}
