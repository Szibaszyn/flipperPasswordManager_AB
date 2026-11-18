#pragma once

#include <gui/scene_manager.h>

// Generate scene on_enter, on_event, on_exit declarations
#define ADD_SCENE(prefix, name, id)                                                     \
    void prefix##_scene_##name##_on_enter(void* context);                                \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);       \
    void prefix##_scene_##name##_on_exit(void* context);

#include "flipper_pwm_scene_config.h"

#undef ADD_SCENE

extern const SceneManagerHandlers flipper_pwm_scene_handlers;
