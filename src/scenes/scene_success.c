#include "../../subghz_gate_scanner.h"
#include "scene_config.h"
#include "../signal_capture.h"
#include "../file_manager.h"
#include "../keeloq.h"

/* ── Protocol detection ──────────────────────────────────────────────────── */

typedef enum {
    ProtoPrinceton,
    ProtoCame,
    ProtoRolling,
    ProtoRAW,
} ProtoType;

static ProtoType detect_proto(const int32_t* buf, uint16_t len) {
    if(len == 0) return ProtoRAW;
    if(len >= 100) return ProtoRolling; /* Keeloq / long rolling-code packet */

    uint32_t min_d = UINT32_MAX, max_d = 0;
    for(uint16_t i = 0; i < len; i++) {
        uint32_t d = (uint32_t)(buf[i] < 0 ? -buf[i] : buf[i]);
        if(d < min_d) min_d = d;
        if(d > max_d) max_d = d;
    }
    /* Princeton: bimodal timing, max/min ratio ~3-4× */
    if(min_d > 0 && max_d <= 5 * min_d && len >= 20 && len <= 65) {
        return ProtoPrinceton;
    }
    if(len > 10 && len < 100) return ProtoCame;
    return ProtoRAW;
}

static const char* proto_name(ProtoType t) {
    switch(t) {
    case ProtoPrinceton: return "Princeton 24-bit";
    case ProtoCame:      return "Came / Nice";
    case ProtoRolling:   return "KeeLoq Rolling";
    default:             return "Unknown (RAW)";
    }
}

static const char* proto_desc(ProtoType t) {
    switch(t) {
    case ProtoPrinceton: return "Fixed — can replay";
    case ProtoCame:      return "Fixed — can replay";
    case ProtoRolling:   return "One-time use only!";
    default:             return "Saved as RAW";
    }
}

/* ── Waveform helper ─────────────────────────────────────────────────────── */

static void draw_waveform(
    Canvas*        canvas,
    const int32_t* buf,
    uint16_t       len,
    uint8_t        x0,
    uint8_t        y0,
    uint8_t        total_w,
    uint8_t        bar_h) {
    if(len == 0 || total_w == 0) return;

    uint32_t total_us = 0;
    for(uint16_t i = 0; i < len; i++) {
        int32_t v = buf[i];
        total_us += (uint32_t)(v < 0 ? -v : v);
    }
    if(total_us == 0) return;

    canvas_draw_line(canvas, x0, (uint8_t)(y0 + bar_h),
                     (uint8_t)(x0 + total_w - 1), (uint8_t)(y0 + bar_h));

    uint16_t x = x0;
    for(uint16_t i = 0; i < len; i++) {
        int32_t  v   = buf[i];
        uint32_t dur = (uint32_t)(v < 0 ? -v : v);
        uint16_t w   = (uint16_t)((uint32_t)dur * total_w / total_us);
        if(w == 0) w = 1;
        if(x + w > (uint16_t)(x0 + total_w)) w = (uint16_t)(x0 + total_w) - x;
        if(w == 0) break;
        if(v > 0) canvas_draw_box(canvas, (uint8_t)x, y0, (uint8_t)w, bar_h);
        x += w;
        if(x >= x0 + total_w) break;
    }
}

/* ── Model ───────────────────────────────────────────────────────────────── */

typedef struct {
    char     freq_str[32];
    char     proto_str[24];
    char     desc_str[24];
    char     path_str[128];
    bool     is_error;
    bool     saved;
    bool     is_rolling;
    /* KeeLoq decoded info (valid when is_rolling) */
    KeeLoqResult kl;
    char     kl_sn_str[24];   /* "SN:0x1A2B3C BTN:0" */
    char     kl_id_str[32];   /* "BFT | CNT:1234" or "No key in DB" */
    char     kl_hop_str[24];  /* "Hop:0xABCD1234" */
    /* Waveform for non-rolling signals */
    int32_t  waveform[64];
    uint16_t waveform_len;
} SuccessModel;

/* ── Draw callback ───────────────────────────────────────────────────────── */

static void success_draw_cb(Canvas* canvas, void* model_ptr) {
    SuccessModel* m = model_ptr;
    canvas_clear(canvas);

    if(m->is_error) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "No Signal");
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, m->path_str);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[Back]");
        return;
    }

    /* Inverted header */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 11, AlignCenter, AlignBottom,
        m->is_rolling ? "KeeLoq Frame!" : "Signal Captured!");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, m->freq_str);

    if(m->is_rolling) {
        /* Rolling code view: show decoded frame info */
        canvas_draw_str(canvas, 2, 33, m->kl_sn_str);

        /* Manufacturer / counter row — inverted if key matched */
        if(m->kl.decoded) {
            canvas_draw_box(canvas, 0, 35, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, 44, m->kl_id_str);
        canvas_set_color(canvas, ColorBlack);

        canvas_draw_str(canvas, 2, 54, m->kl_hop_str);
    } else {
        /* Fixed code view: protocol + description + waveform */
        canvas_draw_str(canvas, 2, 34, m->proto_str);
        canvas_draw_str(canvas, 2, 44, m->desc_str);
        draw_waveform(canvas, m->waveform, m->waveform_len, 1, 46, 126, 8);
    }

    canvas_draw_line(canvas, 0, 55, 127, 55);

    if(m->saved) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, m->path_str);
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[OK]=Save  [Back]=Discard");
    }
}

/* ── Input callback ──────────────────────────────────────────────────────── */

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

/* ── Scene callbacks ─────────────────────────────────────────────────────── */

void scene_success_on_enter(void* context) {
    AppState* app = context;

    bool is_error = (app->capture_len == 0 || !signal_capture_is_valid(app));
    bool is_dup   = (!is_error && signal_capture_is_duplicate(app));

    ProtoType proto = is_error ? ProtoRAW : detect_proto(app->capture_buf, app->capture_len);

    /* Run KeeLoq decoding on main thread before locking the model */
    KeeLoqResult kl_result;
    kl_result.decoded = false;
    if(!is_error && !is_dup && proto == ProtoRolling) {
        keeloq_try_decode(app->capture_buf, app->capture_len, &kl_result);
    }

    with_view_model(
        app->view_success,
        SuccessModel* m,
        {
            m->saved      = false;
            m->is_error   = is_error || is_dup;
            m->is_rolling = (proto == ProtoRolling);
            m->kl         = kl_result;

            if(m->is_error) {
                if(is_dup) {
                    snprintf(m->path_str, sizeof(m->path_str), "Duplicate signal");
                } else {
                    snprintf(
                        m->path_str, sizeof(m->path_str), "%s",
                        strlen(app->save_path) > 0 ? app->save_path : "Signal too short.");
                }
                m->waveform_len = 0;
            } else {
                snprintf(
                    m->freq_str, sizeof(m->freq_str), "%.3f MHz  |  %u pulses",
                    (double)app->current_freq_mhz, (unsigned)app->capture_len);
                snprintf(m->proto_str, sizeof(m->proto_str), "%s", proto_name(proto));
                snprintf(m->desc_str,  sizeof(m->desc_str),  "%s", proto_desc(proto));
                m->path_str[0] = '\0';

                if(proto == ProtoRolling) {
                    if(kl_result.hop || kl_result.fix) {
                        /* Frame was extracted successfully */
                        snprintf(m->kl_sn_str, sizeof(m->kl_sn_str),
                                 "SN:%07lX BTN:%X",
                                 (unsigned long)kl_result.serial,
                                 (unsigned)kl_result.btn);
                        if(kl_result.decoded) {
                            snprintf(m->kl_id_str, sizeof(m->kl_id_str),
                                     "%s | CNT:%u",
                                     kl_result.mfr_name, (unsigned)kl_result.cnt);
                        } else {
                            snprintf(m->kl_id_str, sizeof(m->kl_id_str),
                                     "No key in DB");
                        }
                        snprintf(m->kl_hop_str, sizeof(m->kl_hop_str),
                                 "Hop:%08lX", (unsigned long)kl_result.hop);
                    } else {
                        /* Frame extraction failed — can't decode timing */
                        snprintf(m->kl_sn_str, sizeof(m->kl_sn_str), "Cannot parse frame");
                        snprintf(m->kl_id_str, sizeof(m->kl_id_str), "Rolling (RAW saved)");
                        snprintf(m->kl_hop_str, sizeof(m->kl_hop_str), "");
                    }
                    m->waveform_len = 0;
                } else {
                    /* Downsample to 64 pulses for waveform display */
                    uint16_t step = (app->capture_len > 64) ? (app->capture_len / 64) : 1;
                    uint16_t wlen = 0;
                    for(uint16_t i = 0; i < app->capture_len && wlen < 64; i += step) {
                        m->waveform[wlen++] = app->capture_buf[i];
                    }
                    m->waveform_len = wlen;
                }
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
            app->view_success,
            SuccessModel* m,
            {
                m->saved    = ok;
                m->is_error = !ok;
                if(ok) {
                    const char* slash = strrchr(app->save_path, '/');
                    snprintf(
                        m->path_str, sizeof(m->path_str), "%s",
                        slash ? slash + 1 : app->save_path);
                } else {
                    snprintf(m->path_str, sizeof(m->path_str), "Save failed! No SD?");
                }
            },
            true);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneMainMenu);
        return true;
    }

    if(event.event == AppCustomEventSaveError) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneMainMenu);
        return true;
    }

    return false;
}

void scene_success_on_exit(void* context) {
    UNUSED(context);
}

/* ── View allocation helper ──────────────────────────────────────────────── */

void scene_success_view_init(AppState* app) {
    view_set_draw_callback(app->view_success, success_draw_cb);
    view_set_input_callback(app->view_success, success_input_cb);
    view_set_context(app->view_success, app);
    view_allocate_model(app->view_success, ViewModelTypeLocking, sizeof(SuccessModel));
}
