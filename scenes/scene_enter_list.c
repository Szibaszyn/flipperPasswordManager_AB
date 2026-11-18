#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"

static void enter_list_item_callback(void* context, uint32_t index) {
    FlipperPwmApp* app = context;
    app->selected_entry_idx = index;
    scene_manager_next_scene(app->scene_manager, FlipperPwmSceneTypePassword);
}

void flipper_pwm_scene_enter_list_on_enter(void* context) {
    FlipperPwmApp* app = context;

    // Load vault from storage if empty
    if(app->vault.entry_count == 0) {
        vault_load_file(app->storage, PWM_VAULT_FILE_PATH, NULL, &app->vault);
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Password");

    if(app->vault.entry_count == 0) {
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 15, AlignCenter, AlignCenter, FontPrimary, "Vault is Empty");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "No passwords found");
        widget_add_string_element(
            app->widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "Press Back to return");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewWidget);
        return;
    }

    for(size_t i = 0; i < app->vault.entry_count; i++) {
        const char* desc = app->vault.entries[i].description;
        if(desc[0] == '\0') desc = "(Unnamed)";
        submenu_add_item(app->submenu, desc, i, enter_list_item_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewSubmenu);
}

bool flipper_pwm_scene_enter_list_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_pwm_scene_enter_list_on_exit(void* context) {
    FlipperPwmApp* app = context;
    submenu_reset(app->submenu);
}
