#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../cc1101_ext.h"
#include "../frequency_scanner.h"

/* Icon assets generated from images/ by ufbt */
extern const Icon I_ScanStart_25x27;
extern const Icon I_SettingsApp_25x27;

typedef enum {
    MenuItemStartScan = 0,
    MenuItemSettings  = 1,
} MenuItem;

static void menu_callback(void* context, uint32_t index) {
    AppState* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void scene_main_menu_on_enter(void* context) {
    AppState* app = context;
    Menu* menu = app->view_main_menu;
    menu_reset(menu);
    menu_add_item(menu, "Start Scan", &I_ScanStart_25x27,  MenuItemStartScan, menu_callback, app);
    menu_add_item(menu, "Settings",   &I_SettingsApp_25x27, MenuItemSettings,  menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewMainMenu);
}

bool scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    AppState* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MenuItemStartScan) {
        if(app->antenna_mode == AntennaExternal) {
            /* For external CC1101, ensure module is present */
            if(!app->cc1101_present) {
                app->cc1101_present = cc1101_ext_init(app);
            }
            if(!app->cc1101_present) {
                snprintf(app->save_path, sizeof(app->save_path),
                         "CC1101 not found!\nCheck SPI wiring.");
                scene_manager_next_scene(app->scene_manager, SceneSuccess);
                return true;
            }
        }
        freq_scanner_start(app);
        furi_timer_start(app->scan_timer, furi_ms_to_ticks(FREQ_SCANNER_DWELL_MS));
        scene_manager_next_scene(app->scene_manager, SceneScanning);
        return true;
    }

    if(event.event == MenuItemSettings) {
        scene_manager_next_scene(app->scene_manager, SceneSettings);
        return true;
    }

    return false;
}

void scene_main_menu_on_exit(void* context) {
    AppState* app = context;
    menu_reset(app->view_main_menu);
}
