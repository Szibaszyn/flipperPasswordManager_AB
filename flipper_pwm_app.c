#include "flipper_pwm_app.h"
#include "scenes/flipper_pwm_scene.h"

static bool flipper_pwm_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipperPwmApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flipper_pwm_app_back_event_callback(void* context) {
    furi_assert(context);
    FlipperPwmApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

FlipperPwmApp* flipper_pwm_app_alloc(void) {
    FlipperPwmApp* app = malloc(sizeof(FlipperPwmApp));
    memset(app, 0, sizeof(FlipperPwmApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&flipper_pwm_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flipper_pwm_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipper_pwm_app_back_event_callback);

    // Submenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipperPwmViewSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipperPwmViewWidget, widget_get_view(app->widget));

    // Combo Input View
    app->combo_view = combo_input_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperPwmViewComboInput,
        combo_input_view_get_view(app->combo_view));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Ensure app data directory exists
    storage_simply_mkdir(app->storage, PWM_APP_DATA_DIR);

    scene_manager_next_scene(app->scene_manager, FlipperPwmSceneMainMenu);

    return app;
}

void flipper_pwm_app_free(FlipperPwmApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperPwmViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperPwmViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, FlipperPwmViewComboInput);

    submenu_free(app->submenu);
    widget_free(app->widget);
    combo_input_view_free(app->combo_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t flipper_pwm_app(void* p) {
    UNUSED(p);
    FlipperPwmApp* app = flipper_pwm_app_alloc();

    view_dispatcher_run(app->view_dispatcher);

    flipper_pwm_app_free(app);
    return 0;
}
