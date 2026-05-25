#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../signal_capture.h"
#include "../file_manager.h"

typedef struct {
    char freq_str[24];
    char pulse_str[24];
    char path_str[64];
    bool is_error;
    bool saved;
} SuccessModel;

/* ── Draw callback ──────────────────────────────────────────────────────── */

static void success_draw_cb(Canvas* canvas, void* model_ptr) {
    SuccessModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    if(m->is_error) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 14, "Error");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, m->path_str);
        canvas_draw_str(canvas, 2, 56, "[Back]");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Signal Captured!");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, m->freq_str);
    canvas_draw_str(canvas, 2, 34, m->pulse_str);

    if(m->saved) {
        canvas_draw_str(canvas, 2, 45, "Saved:");
        canvas_draw_str(canvas, 2, 55, m->path_str);
    } else {
        canvas_draw_str(canvas, 2, 48, "[OK]=Save  [Back]=Discard");
    }
}

/* ── Input callback ─────────────────────────────────────────────────────── */

static bool success_input_cb(InputEvent* event, void* context) {
    AppState* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventSaveOk);
        return true;
    }
    if(event->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventSaveError);
        return true;
    }
    return false;
}

/* ── Scene callbacks ────────────────────────────────────────────────────── */

void scene_success_on_enter(void* context) {
    AppState* app = context;

    bool is_error = (app->capture_len == 0 || !signal_capture_is_valid(app));
    bool is_dup   = (!is_error && signal_capture_is_duplicate(app));

    with_view_model(
        app->view_success, SuccessModel* m, {
            m->saved    = false;
            m->is_error = is_error || is_dup;
            if(m->is_error) {
                if(is_dup) {
                    snprintf(m->path_str, sizeof(m->path_str), "Duplicate signal");
                } else {
                    snprintf(m->path_str, sizeof(m->path_str),
                             strlen(app->save_path) > 0 ? app->save_path : "Signal too short.");
                }
            } else {
                snprintf(m->freq_str,  sizeof(m->freq_str),  "Freq: %.3f MHz",
                         (double)app->current_freq_mhz);
                snprintf(m->pulse_str, sizeof(m->pulse_str), "Pulses: %u",
                         (unsigned)app->capture_len);
                m->path_str[0] = '\0';
            }
        },
        true);

    view_set_input_callback(app->view_success, success_input_cb);
    view_set_context(app->view_success, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSuccess);
}

bool scene_success_on_event(void* context, SceneManagerEvent event) {
    AppState* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == AppCustomEventSaveOk) {
        bool ok = file_manager_save(app);
        with_view_model(
            app->view_success, SuccessModel* m, {
                m->saved    = ok;
                m->is_error = !ok;
                if(ok) {
                    const char* slash = strrchr(app->save_path, '/');
                    snprintf(m->path_str, sizeof(m->path_str), "%s",
                             slash ? slash + 1 : app->save_path);
                } else {
                    snprintf(m->path_str, sizeof(m->path_str), "Save failed! No SD?");
                }
            },
            true);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SceneMainMenu);
        return true;
    }

    if(event.event == AppCustomEventSaveError) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SceneMainMenu);
        return true;
    }

    return false;
}

void scene_success_on_exit(void* context) {
    UNUSED(context);
}

/* ── View allocation helper ─────────────────────────────────────────────── */

void scene_success_view_init(AppState* app) {
    view_set_draw_callback(app->view_success, success_draw_cb);
    view_set_input_callback(app->view_success, success_input_cb);
    view_set_context(app->view_success, app);
    view_allocate_model(app->view_success, ViewModelTypeLocking, sizeof(SuccessModel));
}
