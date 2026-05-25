#pragma once

#include <gui/scene_manager.h>

typedef enum {
    SceneMainMenu  = 0,
    SceneScanning  = 1,
    SceneSuccess   = 2,
    SceneSettings  = 3,
    SceneCount,
} SceneId;

/* Forward declarations — implemented in scene_*.c */
void scene_main_menu_on_enter(void* context);
bool scene_main_menu_on_event(void* context, SceneManagerEvent event);
void scene_main_menu_on_exit(void* context);

void scene_scanning_on_enter(void* context);
bool scene_scanning_on_event(void* context, SceneManagerEvent event);
void scene_scanning_on_exit(void* context);

void scene_success_on_enter(void* context);
bool scene_success_on_event(void* context, SceneManagerEvent event);
void scene_success_on_exit(void* context);

void scene_settings_on_enter(void* context);
bool scene_settings_on_event(void* context, SceneManagerEvent event);
void scene_settings_on_exit(void* context);

static const SceneManagerHandlers scene_event_handlers = {
    .handlers = {
        [SceneMainMenu] = {scene_main_menu_on_enter, scene_main_menu_on_event, scene_main_menu_on_exit},
        [SceneScanning] = {scene_scanning_on_enter,  scene_scanning_on_event,  scene_scanning_on_exit},
        [SceneSuccess]  = {scene_success_on_enter,   scene_success_on_event,   scene_success_on_exit},
        [SceneSettings] = {scene_settings_on_enter,  scene_settings_on_event,  scene_settings_on_exit},
    },
    .scene_num = SceneCount,
};
