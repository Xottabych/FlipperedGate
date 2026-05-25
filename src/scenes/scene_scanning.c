#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../frequency_scanner.h"
#include "../signal_capture.h"

typedef struct {
    float   freq_mhz;
    int8_t  rssi;
    uint8_t freq_index;
    uint8_t freq_total;
    char    status[48];
} ScanningModel;

/* ── Draw callback (GUI thread) ─────────────────────────────────────────── */

static void scanning_draw_cb(Canvas* canvas, void* model_ptr) {
    ScanningModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Gate Scanner");

    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_line(canvas, 0, 14, 127, 14);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, m->status);

    /* RSSI bar: -120 dBm = 0%, -20 dBm = 100%, scaled to 100 px */
    int32_t rssi_clamped = m->rssi;
    if(rssi_clamped < -120) rssi_clamped = -120;
    if(rssi_clamped > -20)  rssi_clamped = -20;
    int bar_w = (int)((rssi_clamped + 120) * 100 / 100);
    canvas_draw_str(canvas, 2, 37, "RSSI");
    canvas_draw_frame(canvas, 28, 29, 96, 10);
    if(bar_w > 0) {
        canvas_draw_box(canvas, 29, 30, bar_w, 8);
    }

    /* Frequency progress */
    char prog[16];
    snprintf(prog, sizeof(prog), "%u/%u", (unsigned)(m->freq_index + 1),
             (unsigned)m->freq_total);
    canvas_draw_str(canvas, 2, 52, "Freq:");
    canvas_draw_str(canvas, 32, 52, prog);

    canvas_draw_str(canvas, 70, 52, "[Back]=stop");
}

/* ── Input callback ─────────────────────────────────────────────────────── */

static bool scanning_input_cb(InputEvent* event, void* context) {
    AppState* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher,
                                         (uint32_t)AppCustomEventScanTick + 100);
    }
    return true;
}

/* ── Scene callbacks ────────────────────────────────────────────────────── */

void scene_scanning_on_enter(void* context) {
    AppState* app = context;
    app->capture_len       = 0;
    app->capturing         = false;
    app->capture_done_flag = false;

    /* Determine table size for display */
    uint8_t total = freq_table_all_count;
    switch(app->freq_range) {
    case FreqRange_433: total = freq_table_433_count; break;
    case FreqRange_315: total = freq_table_315_count; break;
    default: break;
    }

    with_view_model(
        app->view_scanning, ScanningModel* m, {
            m->freq_mhz   = app->current_freq_mhz;
            m->rssi       = -120;
            m->freq_index = 0;
            m->freq_total = total;
            snprintf(m->status, sizeof(m->status), "Starting...");
        },
        true);

    view_set_input_callback(app->view_scanning, scanning_input_cb);
    view_set_context(app->view_scanning, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewScanning);
}

bool scene_scanning_on_event(void* context, SceneManagerEvent event) {
    AppState* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    /* Back shortcut from input callback */
    if(event.event == (uint32_t)AppCustomEventScanTick + 100) {
        furi_timer_stop(app->scan_timer);
        freq_scanner_stop(app);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.event == AppCustomEventScanTick) {
        /* Check if ISR set capture_done */
        if(__atomic_load_n(&app->capture_done_flag, __ATOMIC_ACQUIRE)) {
            __atomic_store_n(&app->capture_done_flag, false, __ATOMIC_RELEASE);
            furi_timer_stop(app->scan_timer);
            signal_capture_stop(app);
            scene_manager_next_scene(app->scene_manager, SceneSuccess);
            return true;
        }

        uint8_t total = freq_table_all_count;
        switch(app->freq_range) {
        case FreqRange_433: total = freq_table_433_count; break;
        case FreqRange_315: total = freq_table_315_count; break;
        default: break;
        }

        with_view_model(
            app->view_scanning, ScanningModel* m, {
                m->freq_mhz   = app->current_freq_mhz;
                m->rssi       = app->current_rssi;
                m->freq_index = app->freq_index;
                m->freq_total = total;
                snprintf(m->status, sizeof(m->status), "%.3f MHz  %d dBm",
                         (double)app->current_freq_mhz, (int)app->current_rssi);
            },
            true);
        return true;
    }

    if(event.event == AppCustomEventSignalFound) {
        /* Scanner locked — start raw capture */
        signal_capture_start(app);
        with_view_model(
            app->view_scanning, ScanningModel* m, {
                snprintf(m->status, sizeof(m->status), "LOCKED  %.3f MHz",
                         (double)app->current_freq_mhz);
            },
            true);
        return true;
    }

    return false;
}

void scene_scanning_on_exit(void* context) {
    AppState* app = context;
    furi_timer_stop(app->scan_timer);
    signal_capture_stop(app);
    freq_scanner_stop(app);
    app->capture_done_flag = false;
}

/* ── View allocation helper (called from main) ──────────────────────────── */

void scene_scanning_view_init(AppState* app) {
    view_set_draw_callback(app->view_scanning, scanning_draw_cb);
    view_set_input_callback(app->view_scanning, scanning_input_cb);
    view_set_context(app->view_scanning, app);
    view_allocate_model(app->view_scanning, ViewModelTypeLocking, sizeof(ScanningModel));
}
