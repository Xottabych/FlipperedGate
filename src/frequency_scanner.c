#include "frequency_scanner.h"

/* ── Frequency tables ────────────────────────────────────────────────────── */

const FreqEntry freq_table_all[] = {
    {315000000}, {390000000}, {418000000},
    {433075000}, {433920000}, {434420000}, {434775000},
    {868350000}, {868950000}, {915000000},
};
const uint8_t freq_table_all_count = 10;

const FreqEntry freq_table_433[] = {
    {433075000}, {433420000}, {433920000}, {434420000}, {434775000},
};
const uint8_t freq_table_433_count = 5;

const FreqEntry freq_table_868[] = {
    {868300000}, {868350000}, {868950000},
};
const uint8_t freq_table_868_count = 3;

const FreqEntry freq_table_315[] = {
    {315000000}, {318000000}, {390000000}, {418000000},
};
const uint8_t freq_table_315_count = 4;

/* ── Internal helpers ────────────────────────────────────────────────────── */

static void scanner_select_table(AppState* app, const FreqEntry** table, uint8_t* count) {
    switch(app->freq_range) {
    case FreqRange_433:
        *table = freq_table_433;
        *count = freq_table_433_count;
        break;
    case FreqRange_315:
        *table = freq_table_315;
        *count = freq_table_315_count;
        break;
    case FreqRange_All:
    default:
        *table = freq_table_all;
        *count = freq_table_all_count;
        break;
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void freq_scanner_init(AppState* app) {
    app->freq_index = 0;
    app->current_rssi = -120;

    const FreqEntry* table;
    uint8_t count;
    scanner_select_table(app, &table, &count);

    /* Pre-select first frequency */
    if(app->cc1101_present) {
        cc1101_set_frequency(app, table[0].freq_hz / 1000000.0f);
        app->current_freq_mhz = table[0].freq_hz / 1000000.0f;
        app->capture_freq_hz  = table[0].freq_hz;
        cc1101_enter_rx(app);
    }
    app->dwell_start_tick = furi_get_tick();
}

void freq_scanner_start(AppState* app) {
    freq_scanner_init(app);
    /* Timer created and started in main app; tick calls freq_scanner_tick */
}

void freq_scanner_stop(AppState* app) {
    if(app->cc1101_present) {
        cc1101_idle(app);
    }
    app->capturing = false;
    app->capture_done_flag = false;
}

void freq_scanner_tick(void* context) {
    AppState* app = context;

    /* This callback is invoked from a FuriTimer; must only post events,
       never call GUI functions directly.  SPI is allowed here because the
       Flipper OS timer task has an adequate stack for ~64 µs SPI ops. */

    if(!app->cc1101_present) return;

    const FreqEntry* table;
    uint8_t count;
    scanner_select_table(app, &table, &count);

    /* Read current RSSI */
    int8_t rssi = cc1101_get_rssi(app);
    app->current_rssi = rssi;

    /* Post scan tick event so scanning scene can redraw */
    view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventScanTick);

    /* Check capture_done flag set by GDO0 ISR */
    if(__atomic_load_n(&app->capture_done_flag, __ATOMIC_ACQUIRE)) {
        return; /* Success scene transition handled in main thread */
    }

    if(app->capturing) return; /* Waiting for capture completion */

    /* Check RSSI threshold */
    if(rssi > app->rssi_threshold) {
        /* Lock on this frequency and signal capture start */
        app->capture_freq_hz  = table[app->freq_index].freq_hz;
        app->capturing = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventSignalFound);
        return;
    }

    /* Advance to next frequency after dwell period */
    uint32_t now = furi_get_tick();
    if((now - app->dwell_start_tick) >= FREQ_SCANNER_DWELL_MS) {
        app->freq_index = (app->freq_index + 1) % count;
        float freq_mhz = table[app->freq_index].freq_hz / 1000000.0f;
        cc1101_set_frequency(app, freq_mhz);
        cc1101_enter_rx(app);
        app->current_freq_mhz = freq_mhz;
        app->dwell_start_tick = now;
    }
}
