#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB HID Keyboard emulation.
 */
bool hid_keyboard_start(void);

/**
 * @brief Stop USB HID Keyboard and restore previous USB mode.
 */
void hid_keyboard_stop(void);

/**
 * @brief Checks if USB HID is connected to a host.
 */
bool hid_keyboard_is_connected(void);

/**
 * @brief Types a string character-by-character as USB keyboard keystrokes.
 *
 * @param str Null-terminated string to type
 * @param delay_ms Delay between keypress and keyrelease in milliseconds
 */
bool hid_keyboard_type_string(const char* str, uint32_t delay_ms);

/**
 * @brief Sends Return/Enter keypress.
 */
bool hid_keyboard_press_enter(uint32_t delay_ms);

#ifdef __cplusplus
}
#endif
