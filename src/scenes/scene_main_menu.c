#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../cc1101_ext.h"
#include "../frequency_scanner.h"
#include "../app_icons.h"

#define MENU_ITEM_COUNT 2

typedef enum {
    MenuItemStartScan = 0,
    MenuItemSettings  = 1,
} MenuItem;

typedef struct {
    uint8_t selected;
} MenuModel;

/* ── Draw callback ────────────────────────────────────────────────────────── */

static void menu_draw_cb(Canvas* canvas, void* model) {
    MenuModel* m = model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Inverted title bar */
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "Flipp the Gate");
    canvas_set_color(canvas, ColorBlack);

    /* Thin separator between the two items */
    canvas_draw_line(canvas, 0, 38, 127, 38);

    /* Item data */
    static const uint8_t* const icons[MENU_ITEM_COUNT] = {
        I_ScanStart_xbm,
        I_SettingsApp_xbm,
    };
    static const char* const labels[MENU_ITEM_COUNT] = {
        "Start Scan",
        "Settings",
    };
    /* Top y-coordinate of each item row */
    static const uint8_t tops[MENU_ITEM_COUNT] = { 14, 39 };

    for(uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
        uint8_t y   = tops[i];
        bool    sel = (m->selected == i);

        if(sel) {
            /* Highlight with a filled rounded box, then switch to white for content */
            canvas_draw_rbox(canvas, 0, y, 128, 24, 2);
            canvas_set_color(canvas, ColorWhite);
        }

        /* Icon — draw first 22 rows of the 25×27 XBM, centred vertically */
        canvas_draw_xbm(canvas, 3, y + 1, I_ScanStart_W, 22, icons[i]);

        /* Label */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 33, y + 16, labels[i]);

        canvas_set_color(canvas, ColorBlack);
    }
}

/* ── Input callback ───────────────────────────────────────────────────────── */

static bool menu_input_cb(InputEvent* event, void* context) {
    AppState* app = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    /* Back bubbles up to the navigation callback → scene_manager_handle_back_event */
    if(event->key == InputKeyBack) return false;

    bool consumed = false;
    with_view_model(
        app->view_main_menu,
        MenuModel* m,
        {
            if(event->key == InputKeyUp) {
                m->selected = (m->selected == 0) ? MENU_ITEM_COUNT - 1 : m->selected - 1;
                consumed    = true;
            } else if(event->key == InputKeyDown) {
                m->selected = (m->selected + 1) % MENU_ITEM_COUNT;
                consumed    = true;
            } else if(event->key == InputKeyOk) {
                view_dispatcher_send_custom_event(app->view_dispatcher, m->selected);
                consumed = true;
            }
        },
        consumed);

    return consumed;
}

/* ── View initialiser — called once from app_alloc ───────────────────────── */

void scene_main_menu_view_init(AppState* app) {
    view_set_draw_callback(app->view_main_menu, menu_draw_cb);
    view_set_input_callback(app->view_main_menu, menu_input_cb);
    view_set_context(app->view_main_menu, app);
    view_allocate_model(app->view_main_menu, ViewModelTypeLocking, sizeof(MenuModel));
}

/* ── Scene lifecycle ──────────────────────────────────────────────────────── */

void scene_main_menu_on_enter(void* context) {
    AppState* app = context;
    with_view_model(
        app->view_main_menu,
        MenuModel* m,
        { m->selected = 0; },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewMainMenu);
}

bool scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    AppState* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MenuItemStartScan) {
        if(app->antenna_mode == AntennaExternal) {
            if(!app->cc1101_present) {
                app->cc1101_present = cc1101_ext_init(app);
            }
            if(!app->cc1101_present) {
                snprintf(
                    app->save_path,
                    sizeof(app->save_path),
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
    UNUSED(context);
}
