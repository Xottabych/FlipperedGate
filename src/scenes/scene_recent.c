#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../file_manager.h"

static void recent_cb(void* context, uint32_t index) {
    UNUSED(index);
    AppState* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void scene_recent_on_enter(void* context) {
    AppState* app = context;
    Submenu*  sub = app->view_recent;
    submenu_reset(sub);
    submenu_set_header(sub, "Recent Captures");

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    dir     = storage_file_alloc(storage);

    app->recent_files_count = 0;

    if(storage_dir_open(dir, FILE_MANAGER_SAVE_DIR)) {
        FileInfo fi;
        char     name[64];
        while(storage_dir_read(dir, &fi, name, sizeof(name)) &&
              app->recent_files_count < RECENT_FILES_MAX) {
            if(strstr(name, ".sub")) {
                strncpy(
                    app->recent_files[app->recent_files_count], name,
                    sizeof(app->recent_files[0]) - 1);
                app->recent_files[app->recent_files_count]
                                 [sizeof(app->recent_files[0]) - 1] = '\0';
                app->recent_files_count++;
            }
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    if(app->recent_files_count == 0) {
        submenu_add_item(sub, "No captures yet", 0, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->recent_files_count; i++) {
            submenu_add_item(sub, app->recent_files[i], i, recent_cb, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewRecent);
}

bool scene_recent_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void scene_recent_on_exit(void* context) {
    AppState* app = context;
    submenu_reset(app->view_recent);
}
