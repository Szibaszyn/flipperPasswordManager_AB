#include "hid_keyboard.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

static FuriHalUsbInterface* g_usb_prev = NULL;
static bool g_hid_active = false;

bool hid_keyboard_start(void) {
    if(g_hid_active) return true;

    g_usb_prev = furi_hal_usb_get_config();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        return false;
    }

    g_hid_active = true;
    // Allow USB enumeration to settle
    furi_delay_ms(500);
    return true;
}

void hid_keyboard_stop(void) {
    if(!g_hid_active) return;
    furi_hal_hid_kb_release_all();
    furi_delay_ms(50);
    furi_hal_usb_set_config(g_usb_prev, NULL);
    g_usb_prev = NULL;
    g_hid_active = false;
}

bool hid_keyboard_is_connected(void) {
    return furi_hal_hid_is_connected();
}

bool hid_keyboard_type_string(const char* str, uint32_t delay_ms) {
    if(!str || !g_hid_active) return false;
    if(delay_ms == 0) delay_ms = 15;

    for(size_t i = 0; str[i] != '\0'; i++) {
        uint8_t c = (uint8_t)str[i];
        uint16_t key = HID_ASCII_TO_KEY(c);

        if(key != HID_KEYBOARD_NONE) {
            furi_hal_hid_kb_press(key);
            furi_delay_ms(delay_ms);
            furi_hal_hid_kb_release(key);
            furi_delay_ms(delay_ms);
        }
    }

    furi_hal_hid_kb_release_all();
    return true;
}

bool hid_keyboard_press_enter(uint32_t delay_ms) {
    if(!g_hid_active) return false;
    if(delay_ms == 0) delay_ms = 20;

    furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
    furi_delay_ms(delay_ms);
    furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
    furi_delay_ms(delay_ms);
    return true;
}
