#pragma once

#include <gui/scene_manager.h>

typedef enum {
    SceneMainMenu  = 0,
    SceneScanning  = 1,
    SceneSuccess   = 2,
    SceneSettings  = 3,
    SceneRecent    = 4,
    SceneDecode    = 5,
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

void scene_recent_on_enter(void* context);
bool scene_recent_on_event(void* context, SceneManagerEvent event);
void scene_recent_on_exit(void* context);

void scene_decode_on_enter(void* context);
bool scene_decode_on_event(void* context, SceneManagerEvent event);
void scene_decode_on_exit(void* context);

static void (*const scene_on_enter_handlers[])(void*) = {
    [SceneMainMenu] = scene_main_menu_on_enter,
    [SceneScanning] = scene_scanning_on_enter,
    [SceneSuccess]  = scene_success_on_enter,
    [SceneSettings] = scene_settings_on_enter,
    [SceneRecent]   = scene_recent_on_enter,
    [SceneDecode]   = scene_decode_on_enter,
};

static bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    [SceneMainMenu] = scene_main_menu_on_event,
    [SceneScanning] = scene_scanning_on_event,
    [SceneSuccess]  = scene_success_on_event,
    [SceneSettings] = scene_settings_on_event,
    [SceneRecent]   = scene_recent_on_event,
    [SceneDecode]   = scene_decode_on_event,
};

static void (*const scene_on_exit_handlers[])(void*) = {
    [SceneMainMenu] = scene_main_menu_on_exit,
    [SceneScanning] = scene_scanning_on_exit,
    [SceneSuccess]  = scene_success_on_exit,
    [SceneSettings] = scene_settings_on_exit,
    [SceneRecent]   = scene_recent_on_exit,
    [SceneDecode]   = scene_decode_on_exit,
};

static const SceneManagerHandlers scene_event_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers  = scene_on_exit_handlers,
    .scene_num         = SceneCount,
};
