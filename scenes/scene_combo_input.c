#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"

static void combo_input_done_callback(const uint8_t* arrows, size_t count, void* context) {
    UNUSED(arrows);
    UNUSED(count);
    FlipperPwmApp* app = context;

    vault_load_file(app->storage, PWM_VAULT_FILE_PATH, NULL, &app->vault);
    app->vault_unlocked = true;

    if(app->combo_mode == ComboModeUnlockSave) {
        scene_manager_next_scene(app->scene_manager, FlipperPwmSceneSaveMode);
    } else {
        scene_manager_next_scene(app->scene_manager, FlipperPwmSceneEnterList);
    }
}

static void combo_input_cancel_callback(void* context) {
    FlipperPwmApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void flipper_pwm_scene_combo_input_on_enter(void* context) {
    FlipperPwmApp* app = context;

    combo_input_view_reset(app->combo_view);
    combo_input_view_set_header(app->combo_view, "Press OK to Unlock");
    combo_input_view_set_callback(app->combo_view, combo_input_done_callback, app);
    combo_input_view_set_cancel_callback(app->combo_view, combo_input_cancel_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperPwmViewComboInput);
}

bool flipper_pwm_scene_combo_input_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_pwm_scene_combo_input_on_exit(void* context) {
    FlipperPwmApp* app = context;
    combo_input_view_reset(app->combo_view);
}
