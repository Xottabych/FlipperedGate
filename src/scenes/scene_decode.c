#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../sub_file_parser.h"
#include "../file_manager.h"

static void decode_file_cb(void* context, uint32_t index) {
    AppState* app = context;

    if(index >= app->recent_files_count) return;

    /* Build the full path to the selected .sub file */
    char path[128];
    snprintf(path, sizeof(path), "%s/%s",
             FILE_MANAGER_SAVE_DIR, app->recent_files[index]);

    /* Parse the file into app->capture_buf */
    bool parsed = sub_file_parse(app, path);

    if(!parsed) {
        /* Signal parse failure via save_path — SceneSuccess shows "No Signal" */
        snprintf(app->save_path, sizeof(app->save_path), "Cannot read file");
        app->capture_len = 0;
    } else {
        app->save_path[0] = '\0';
        /* Reset duplicate-detection so the same file can be decoded repeatedly */
        app->last_capture_hash = 0;
        app->last_capture_ts   = 0;
    }

    scene_manager_next_scene(app->scene_manager, SceneSuccess);
}

void scene_decode_on_enter(void* context) {
    AppState* app = context;
    Submenu*  sub = app->view_decode;
    submenu_reset(sub);
    submenu_set_header(sub, "Decode .sub File");

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
        submenu_add_item(sub, "No .sub files found", 0, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->recent_files_count; i++) {
            submenu_add_item(sub, app->recent_files[i], i, decode_file_cb, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDecode);
}

bool scene_decode_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void scene_decode_on_exit(void* context) {
    AppState* app = context;
    submenu_reset(app->view_decode);
}
