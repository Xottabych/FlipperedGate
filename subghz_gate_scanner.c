#include "subghz_gate_scanner.h"
#include "src/scenes/scene_config.h"
#include "src/cc1101_ext.h"
#include "src/frequency_scanner.h"
#include "src/signal_capture.h"

/* Declared in their respective scene source files */
extern void scene_main_menu_view_init(AppState* app);
extern void scene_scanning_view_init(AppState* app);
extern void scene_success_view_init(AppState* app);

/* ── Navigation callback ─────────────────────────────────────────────────── */

static bool app_navigation_cb(void* context) {
    AppState* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ── Custom event callback ──────────────────────────────────────────────── */

static bool app_custom_event_cb(void* context, uint32_t custom_event) {
    AppState* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

/* ── Scan timer callback (posts to main thread) ─────────────────────────── */

static void scan_timer_cb(void* context) {
    freq_scanner_tick(context);
}

/* ── Application lifecycle ──────────────────────────────────────────────── */

AppState* app_alloc(void) {
    AppState* app = malloc(sizeof(AppState));
    furi_check(app);
    memset(app, 0, sizeof(AppState));

    /* Allocate capture buffer on heap (too large for stack) */
    app->capture_buf = malloc(CAPTURE_BUFFER_SIZE * sizeof(int32_t));
    furi_check(app->capture_buf);

    /* Default settings */
    app->rssi_threshold    = -85;
    app->squelch_threshold = 30;
    app->freq_range        = FreqRange_433;
    app->dwell_ms          = 200;
    app->scan_mode         = ScanModeSweep;
    app->fixed_freq_hz     = 433920000;
    app->modulation        = ModOOK;

    /* Assign GPIO pins */
    app->pin_mosi = &gpio_ext_pa7;
    app->pin_miso = &gpio_ext_pa6;
    app->pin_cs   = &gpio_ext_pa4;
    app->pin_sck  = &gpio_ext_pb3;
    app->pin_gd0  = &gpio_ext_pb2;

    /* Open SDK services */
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_cb);

    /* Scene manager */
    app->scene_manager = scene_manager_alloc(&scene_event_handlers, app);

    /* Main menu (custom View with canvas_draw_xbm) */
    app->view_main_menu = view_alloc();
    scene_main_menu_view_init(app);
    view_dispatcher_add_view(app->view_dispatcher, AppViewMainMenu, app->view_main_menu);

    /* Scanning (custom View) */
    app->view_scanning = view_alloc();
    scene_scanning_view_init(app);
    view_dispatcher_add_view(app->view_dispatcher, AppViewScanning, app->view_scanning);

    /* Success (custom View) */
    app->view_success = view_alloc();
    scene_success_view_init(app);
    view_dispatcher_add_view(app->view_dispatcher, AppViewSuccess, app->view_success);

    /* Settings (VariableItemList) */
    app->view_settings = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewSettings,
        variable_item_list_get_view(app->view_settings));

    /* Recent files (Submenu) */
    app->view_recent = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewRecent,
        submenu_get_view(app->view_recent));

    /* Attach dispatcher to GUI */
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Periodic timer for frequency hopping */
    app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);

    /* Try to detect CC1101 */
    app->cc1101_present = cc1101_ext_init(app);
    if(!app->cc1101_present) {
        FURI_LOG_W("App", "External CC1101 not detected");
    }

    return app;
}

void app_free(AppState* app) {
    furi_timer_stop(app->scan_timer);
    furi_timer_free(app->scan_timer);

    signal_capture_stop(app);
    freq_scanner_stop(app);

    if(app->cc1101_present) {
        cc1101_ext_deinit(app);
    } else if(app->antenna_mode == AntennaExternal) {
        furi_hal_power_disable_otg();
    }

    view_dispatcher_remove_view(app->view_dispatcher, AppViewRecent);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSuccess);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewScanning);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewMainMenu);

    submenu_free(app->view_recent);
    variable_item_list_free(app->view_settings);
    view_free(app->view_success);
    view_free(app->view_scanning);
    view_free(app->view_main_menu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app->capture_buf);
    free(app);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int32_t subghz_gate_scanner_app(void* p) {
    UNUSED(p);

    AppState* app = app_alloc();

    scene_manager_next_scene(app->scene_manager, SceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
