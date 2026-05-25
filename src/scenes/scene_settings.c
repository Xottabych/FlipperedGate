#include "../../subghz_gate_scanner.h"
#include "scene_config.h"

/* RSSI threshold options: -60 to -110 dBm in 5 dBm steps (11 entries) */
static const int8_t rssi_values[] = {
    -60, -65, -70, -75, -80, -85, -90, -95, -100, -105, -110
};
#define RSSI_COUNT 11

static const char* const freq_range_labels[] = {
    "All Bands", "433 MHz", "315 MHz",
};
#define FREQ_RANGE_COUNT 3

static const char* const antenna_labels[] = { "External", "Built-in" };
#define ANTENNA_COUNT 2

/* ── Callbacks ──────────────────────────────────────────────────────────── */

static void rssi_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= RSSI_COUNT) idx = 5; /* default -85 */
    app->rssi_threshold = rssi_values[idx];
    char val[10];
    snprintf(val, sizeof(val), "%d dBm", (int)app->rssi_threshold);
    variable_item_set_current_value_text(item, val);
}

static void freq_range_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= FREQ_RANGE_COUNT) idx = 0;
    app->freq_range = (FreqRange)idx;
    variable_item_set_current_value_text(item, freq_range_labels[idx]);
}

static void squelch_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->squelch_threshold = (int8_t)((idx + 1) * 10);
    char val[8];
    snprintf(val, sizeof(val), "%d", (int)app->squelch_threshold);
    variable_item_set_current_value_text(item, val);
}

static void antenna_change_cb(VariableItem* item) {
    AppState* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= ANTENNA_COUNT) idx = 0;
    app->antenna_mode = (AntennaMode)idx;
    variable_item_set_current_value_text(item, antenna_labels[idx]);
}

/* ── Scene callbacks ────────────────────────────────────────────────────── */

void scene_settings_on_enter(void* context) {
    AppState* app = context;
    VariableItemList* list = app->view_settings;
    variable_item_list_reset(list);

    /* RSSI Threshold */
    VariableItem* item;
    item = variable_item_list_add(list, "RSSI Threshold", RSSI_COUNT, rssi_change_cb, app);
    /* Find current index */
    uint8_t rssi_idx = 5; /* default */
    for(uint8_t i = 0; i < RSSI_COUNT; i++) {
        if(rssi_values[i] == app->rssi_threshold) { rssi_idx = i; break; }
    }
    variable_item_set_current_value_index(item, rssi_idx);
    char rssi_str[10];
    snprintf(rssi_str, sizeof(rssi_str), "%d dBm", (int)app->rssi_threshold);
    variable_item_set_current_value_text(item, rssi_str);

    /* Frequency Range */
    item = variable_item_list_add(list, "Freq Range", FREQ_RANGE_COUNT, freq_range_change_cb, app);
    variable_item_set_current_value_index(item, (uint8_t)app->freq_range);
    variable_item_set_current_value_text(item, freq_range_labels[(uint8_t)app->freq_range]);

    /* Squelch level (1–10, maps to 10–100 minimum pulses) */
    item = variable_item_list_add(list, "Squelch", 10, squelch_change_cb, app);
    uint8_t sq_idx = (uint8_t)((app->squelch_threshold > 0 ? app->squelch_threshold : 30) / 10);
    if(sq_idx == 0) sq_idx = 1;
    if(sq_idx > 10) sq_idx = 10;
    variable_item_set_current_value_index(item, sq_idx - 1);
    char sq_str[8];
    snprintf(sq_str, sizeof(sq_str), "%d", (int)app->squelch_threshold);
    variable_item_set_current_value_text(item, sq_str);

    /* Antenna selection */
    item = variable_item_list_add(
        list, "Antenna", ANTENNA_COUNT, antenna_change_cb, app);
    uint8_t ant_idx = (uint8_t)app->antenna_mode;
    if(ant_idx >= ANTENNA_COUNT) ant_idx = 0;
    variable_item_set_current_value_index(item, ant_idx);
    variable_item_set_current_value_text(item, antenna_labels[ant_idx]);

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
