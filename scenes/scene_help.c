#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"

static void help_btn_callback(GuiButtonType result, InputType type, void* context) {
    FlipperPwmApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeLeft) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

void flipper_pwm_scene_help_on_enter(void* context) {
    FlipperPwmApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Password Manager");
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary, "Line: pass:@:description");
    widget_add_string_element(
        app->widget, 64, 28, AlignCenter, AlignTop, FontSecondary, "Arrows: ^(00) v(01) <(10) >(11)");
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignTop, FontSecondary, "Encrypted via ChaCha20");

    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Back", help_btn_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewWidget);
}

bool flipper_pwm_scene_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_pwm_scene_help_on_exit(void* context) {
    FlipperPwmApp* app = context;
    widget_reset(app->widget);
}
