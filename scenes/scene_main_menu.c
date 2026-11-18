#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"

enum MainMenuItemIndex {
    MainMenuItemSaveMode,
    MainMenuItemEnterMode,
    MainMenuItemHelp,
};

static void main_menu_callback(void* context, uint32_t index) {
    FlipperPwmApp* app = context;

    switch(index) {
    case MainMenuItemSaveMode:
        app->combo_mode = ComboModeUnlockSave;
        vault_load_file(app->storage, PWM_VAULT_FILE_PATH, NULL, &app->vault);
        scene_manager_next_scene(app->scene_manager, FlipperPwmSceneSaveMode);
        break;
    case MainMenuItemEnterMode:
        app->combo_mode = ComboModeUnlockEnter;
        vault_load_file(app->storage, PWM_VAULT_FILE_PATH, NULL, &app->vault);
        scene_manager_next_scene(app->scene_manager, FlipperPwmSceneEnterList);
        break;
    case MainMenuItemHelp:
        scene_manager_next_scene(app->scene_manager, FlipperPwmSceneHelp);
        break;
    default:
        break;
    }
}

void flipper_pwm_scene_main_menu_on_enter(void* context) {
    FlipperPwmApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Password Manager");

    submenu_add_item(
        app->submenu, "Save mode (USB Drive)", MainMenuItemSaveMode, main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Enter mode (HID Type)", MainMenuItemEnterMode, main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Help & About", MainMenuItemHelp, main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewSubmenu);
}

bool flipper_pwm_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_pwm_scene_main_menu_on_exit(void* context) {
    FlipperPwmApp* app = context;
    submenu_reset(app->submenu);
}
