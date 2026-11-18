#include "../flipper_pwm_app.h"
#include "flipper_pwm_scene.h"

// Generate handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const flipper_pwm_scene_on_enter_handlers[])(void*) = {
#include "flipper_pwm_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const flipper_pwm_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "flipper_pwm_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const flipper_pwm_scene_on_exit_handlers[])(void*) = {
#include "flipper_pwm_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers flipper_pwm_scene_handlers = {
    .on_enter_handlers = flipper_pwm_scene_on_enter_handlers,
    .on_event_handlers = flipper_pwm_scene_on_event_handlers,
    .on_exit_handlers = flipper_pwm_scene_on_exit_handlers,
    .scene_num = FlipperPwmSceneNum,
};
