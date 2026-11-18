#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"
#include "../usb/hid_keyboard.h"
#include <notification/notification_messages.h>

static void type_password_btn_callback(GuiButtonType result, InputType type, void* context) {
    FlipperPwmApp* app = context;
    if(type == InputTypeShort) {
        if(result == GuiButtonTypeCenter) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PwmEventTypeDone);
        } else if(result == GuiButtonTypeLeft) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }
}

void flipper_pwm_scene_type_password_on_enter(void* context) {
    FlipperPwmApp* app = context;

    widget_reset(app->widget);

    if(app->selected_entry_idx >= app->vault.entry_count) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    const char* desc = app->vault.entries[app->selected_entry_idx].description;

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Ready to Type");

    char target_str[48];
    snprintf(target_str, sizeof(target_str), "Target: %.32s", desc);
    widget_add_string_element(
        app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary, target_str);

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary, "Focus input field on PC");

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Type!", type_password_btn_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", type_password_btn_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewWidget);
}

bool flipper_pwm_scene_type_password_on_event(void* context, SceneManagerEvent event) {
    FlipperPwmApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == PwmEventTypeDone) {
        if(app->selected_entry_idx < app->vault.entry_count) {
            const char* pwd = app->vault.entries[app->selected_entry_idx].password;

            hid_keyboard_start();
            furi_delay_ms(300);

            hid_keyboard_type_string(pwd, 15);

            hid_keyboard_stop();

            // Vibrate and LED success
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_success);
            furi_record_close(RECORD_NOTIFICATION);

            // Show completion
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 14, AlignCenter, AlignCenter, FontPrimary, "Password Entered!");
            widget_add_string_element(
                app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Keyboard emulation done");
            widget_add_string_element(
                app->widget, 64, 46, AlignCenter, AlignCenter, FontSecondary, "Press Back to return");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewWidget);
        }
        consumed = true;
    }

    return consumed;
}

void flipper_pwm_scene_type_password_on_exit(void* context) {
    FlipperPwmApp* app = context;
    widget_reset(app->widget);
}
