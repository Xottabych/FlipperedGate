#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../cc1101_ext.h"
#include "../frequency_scanner.h"

#define MENU_ITEM_COUNT 3

typedef enum {
    MenuItemStartScan = 0,
    MenuItemRecent    = 1,
    MenuItemSettings  = 2,
} MenuItem;

typedef struct {
    uint8_t selected;
    uint8_t antenna_mode;   /* 0=External, 1=Internal */
    bool    cc1101_present;
} MenuModel;

/* ── Draw callback ────────────────────────────────────────────────────────── */

static void menu_draw_cb(Canvas* canvas, void* model) {
    MenuModel* m = (MenuModel*)model;

    canvas_clear(canvas);

    /* ── Inverted header bar (y=0..12) ──────────────────────────────────── */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "Flipp the Gate");

    /* CC1101 status indicator — top-right corner of header */
    canvas_set_font(canvas, FontSecondary);
    const char* hw_label;
    if(m->antenna_mode == AntennaInternal) {
        hw_label = "INT";
    } else if(m->cc1101_present) {
        hw_label = "EXT";
    } else {
        hw_label = "EXT?";
    }
    canvas_draw_str_aligned(canvas, 126, 11, AlignRight, AlignBottom, hw_label);

    canvas_set_color(canvas, ColorBlack);

    /* ── 3 menu rows (each 17px tall, starting at y=13) ────────────────── */
    static const char* const labels[MENU_ITEM_COUNT] = {
        "Start Scan",
        "Recent Files",
        "Settings",
    };

    for(uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
        uint8_t item_y = 13 + (uint8_t)(i * 17);
        bool    sel    = (m->selected == i);

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, item_y, 128, 17, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, item_y + 12, AlignCenter, AlignBottom, labels[i]);

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
        {
            m->selected      = 0;
            m->antenna_mode  = (uint8_t)app->antenna_mode;
            m->cc1101_present = app->cc1101_present;
        },
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
        furi_timer_start(app->scan_timer, furi_ms_to_ticks(app->dwell_ms));
        scene_manager_next_scene(app->scene_manager, SceneScanning);
        return true;
    }

    if(event.event == MenuItemRecent) {
        scene_manager_next_scene(app->scene_manager, SceneRecent);
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
