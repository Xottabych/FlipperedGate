#include "../../subghz_gate_scanner.h"
#include "scene_config.h"

/* ── RSSI threshold ──────────────────────────────────────────────────────── */

static const int8_t rssi_values[] = {
    -60, -65, -70, -75, -80, -85, -90, -95, -100, -105, -110,
};
#define RSSI_COUNT 11

/* ── Frequency range ──────────────────────────────────────────────────────── */

static const char* const freq_range_labels[] = { "All Bands", "433 MHz", "315 MHz" };
#define FREQ_RANGE_COUNT 3

/* ── Antenna ─────────────────────────────────────────────────────────────── */

static const char* const antenna_labels[] = { "External", "Built-in" };
#define ANTENNA_COUNT 2

/* ── Dwell time ──────────────────────────────────────────────────────────── */

static const uint16_t    dwell_values[] = { 100, 200, 500, 1000 };
static const char* const dwell_labels[] = { "100 ms", "200 ms", "500 ms", "1 s" };
#define DWELL_COUNT 4

/* ── Modulation ──────────────────────────────────────────────────────────── */

static const char* const mod_labels[] = { "OOK (AM)", "2-FSK" };
#define MOD_COUNT 2

/* ── Scan mode ───────────────────────────────────────────────────────────── */

static const char* const scan_mode_labels[] = { "Sweep", "Fixed" };
#define SCAN_MODE_COUNT 2

/* ── Fixed frequency options ─────────────────────────────────────────────── */

static const uint32_t    fixed_freq_hz_opts[] = {
    315000000, 390000000, 418000000,
    433920000, 434420000, 434775000,
    868350000, 868950000,
};
static const char* const fixed_freq_labels[] = {
    "315.00", "390.00", "418.00",
    "433.92", "434.42", "434.78",
    "868.35", "868.95",
};
#define FIXED_FREQ_COUNT 8

/* ── Callbacks ──────────────────────────────────────────────────────────── */

static void rssi_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= RSSI_COUNT) idx = 5;
    app->rssi_threshold = rssi_values[idx];
    char val[10];
    snprintf(val, sizeof(val), "%d dBm", (int)app->rssi_threshold);
    variable_item_set_current_value_text(item, val);
}

static void freq_range_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= FREQ_RANGE_COUNT) idx = 0;
    app->freq_range = (FreqRange)idx;
    variable_item_set_current_value_text(item, freq_range_labels[idx]);
}

static void squelch_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    app->squelch_threshold = (int8_t)((idx + 1) * 10);
    char val[8];
    snprintf(val, sizeof(val), "%d", (int)app->squelch_threshold);
    variable_item_set_current_value_text(item, val);
}

static void antenna_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= ANTENNA_COUNT) idx = 0;
    app->antenna_mode = (AntennaMode)idx;
    variable_item_set_current_value_text(item, antenna_labels[idx]);
}

static void dwell_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= DWELL_COUNT) idx = 1;
    app->dwell_ms = dwell_values[idx];
    variable_item_set_current_value_text(item, dwell_labels[idx]);
}

static void mod_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= MOD_COUNT) idx = 0;
    app->modulation = (ModulationMode)idx;
    variable_item_set_current_value_text(item, mod_labels[idx]);
}

static void scan_mode_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= SCAN_MODE_COUNT) idx = 0;
    app->scan_mode = (ScanMode)idx;
    variable_item_set_current_value_text(item, scan_mode_labels[idx]);
}

static void fixed_freq_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t   idx = variable_item_get_current_value_index(item);
    if(idx >= FIXED_FREQ_COUNT) idx = 3;
    app->fixed_freq_hz = fixed_freq_hz_opts[idx];
    variable_item_set_current_value_text(item, fixed_freq_labels[idx]);
}

/* ── Scene lifecycle ─────────────────────────────────────────────────────── */

void scene_settings_on_enter(void* context) {
    AppState*         app  = context;
    VariableItemList* list = app->view_settings;
    variable_item_list_reset(list);

    VariableItem* item;

    /* 1 — RSSI Threshold */
    item = variable_item_list_add(list, "RSSI Threshold", RSSI_COUNT, rssi_change_cb, app);
    uint8_t rssi_idx = 5;
    for(uint8_t i = 0; i < RSSI_COUNT; i++) {
        if(rssi_values[i] == app->rssi_threshold) { rssi_idx = i; break; }
    }
    variable_item_set_current_value_index(item, rssi_idx);
    char rssi_str[10];
    snprintf(rssi_str, sizeof(rssi_str), "%d dBm", (int)app->rssi_threshold);
    variable_item_set_current_value_text(item, rssi_str);

    /* 2 — Frequency Range */
    item = variable_item_list_add(list, "Freq Range", FREQ_RANGE_COUNT, freq_range_change_cb, app);
    variable_item_set_current_value_index(item, (uint8_t)app->freq_range);
    variable_item_set_current_value_text(item, freq_range_labels[(uint8_t)app->freq_range]);

    /* 3 — Squelch */
    item = variable_item_list_add(list, "Squelch", 10, squelch_change_cb, app);
    uint8_t sq_idx = (uint8_t)((app->squelch_threshold > 0 ? app->squelch_threshold : 30) / 10);
    if(sq_idx == 0) sq_idx = 1;
    if(sq_idx > 10) sq_idx = 10;
    variable_item_set_current_value_index(item, sq_idx - 1);
    char sq_str[8];
    snprintf(sq_str, sizeof(sq_str), "%d", (int)app->squelch_threshold);
    variable_item_set_current_value_text(item, sq_str);

    /* 4 — Antenna */
    item = variable_item_list_add(list, "Antenna", ANTENNA_COUNT, antenna_change_cb, app);
    uint8_t ant_idx = (uint8_t)app->antenna_mode;
    if(ant_idx >= ANTENNA_COUNT) ant_idx = 0;
    variable_item_set_current_value_index(item, ant_idx);
    variable_item_set_current_value_text(item, antenna_labels[ant_idx]);

    /* 5 — Dwell Time */
    item = variable_item_list_add(list, "Dwell Time", DWELL_COUNT, dwell_change_cb, app);
    uint8_t dwell_idx = 1;
    for(uint8_t i = 0; i < DWELL_COUNT; i++) {
        if(dwell_values[i] == (uint16_t)app->dwell_ms) { dwell_idx = i; break; }
    }
    variable_item_set_current_value_index(item, dwell_idx);
    variable_item_set_current_value_text(item, dwell_labels[dwell_idx]);

    /* 6 — Modulation */
    item = variable_item_list_add(list, "Modulation", MOD_COUNT, mod_change_cb, app);
    uint8_t mod_idx = (uint8_t)app->modulation;
    if(mod_idx >= MOD_COUNT) mod_idx = 0;
    variable_item_set_current_value_index(item, mod_idx);
    variable_item_set_current_value_text(item, mod_labels[mod_idx]);

    /* 7 — Scan Mode */
    item = variable_item_list_add(list, "Scan Mode", SCAN_MODE_COUNT, scan_mode_change_cb, app);
    uint8_t sm_idx = (uint8_t)app->scan_mode;
    if(sm_idx >= SCAN_MODE_COUNT) sm_idx = 0;
    variable_item_set_current_value_index(item, sm_idx);
    variable_item_set_current_value_text(item, scan_mode_labels[sm_idx]);

    /* 8 — Fixed Frequency */
    item = variable_item_list_add(list, "Fixed Freq", FIXED_FREQ_COUNT, fixed_freq_change_cb, app);
    uint8_t ff_idx = 3;
    for(uint8_t i = 0; i < FIXED_FREQ_COUNT; i++) {
        if(fixed_freq_hz_opts[i] == app->fixed_freq_hz) { ff_idx = i; break; }
    }
    variable_item_set_current_value_index(item, ff_idx);
    variable_item_set_current_value_text(item, fixed_freq_labels[ff_idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSettings);
}

bool scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void scene_settings_on_exit(void* context) {
    AppState* app = context;
    variable_item_list_reset(app->view_settings);
}
