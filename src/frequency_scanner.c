#include "frequency_scanner.h"
#include <furi_hal_subghz.h>

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
    app->freq_index  = 0;
    app->current_rssi = -120;

    const FreqEntry* table;
    uint8_t count;
    scanner_select_table(app, &table, &count);

    if(app->antenna_mode == AntennaInternal) {
        furi_hal_subghz_reset();
        furi_hal_subghz_set_frequency_and_path(table[0].freq_hz);
        furi_hal_subghz_rx();
    } else if(app->cc1101_present) {
        cc1101_set_frequency(app, table[0].freq_hz / 1000000.0f);
        cc1101_enter_rx(app);
    }

    app->current_freq_mhz = table[0].freq_hz / 1000000.0f;
    app->capture_freq_hz  = table[0].freq_hz;
    app->dwell_start_tick = furi_get_tick();
}

void freq_scanner_start(AppState* app) {
    freq_scanner_init(app);
}

void freq_scanner_stop(AppState* app) {
    if(app->antenna_mode == AntennaInternal) {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    } else if(app->cc1101_present) {
        cc1101_idle(app);
    }
    app->capturing         = false;
    app->capture_done_flag = false;
}

void freq_scanner_tick(void* context) {
    AppState* app = context;

    const FreqEntry* table;
    uint8_t count;
    scanner_select_table(app, &table, &count);

    /* Read RSSI from whichever radio is active */
    int8_t rssi;
    if(app->antenna_mode == AntennaInternal) {
        rssi = (int8_t)furi_hal_subghz_get_rssi();
    } else {
        if(!app->cc1101_present) return;
        rssi = cc1101_get_rssi(app);
    }
    app->current_rssi = rssi;

    view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventScanTick);

    if(__atomic_load_n(&app->capture_done_flag, __ATOMIC_ACQUIRE)) return;
    if(app->capturing) return;

    if(rssi > app->rssi_threshold) {
        app->capture_freq_hz = table[app->freq_index].freq_hz;
        app->capturing = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventSignalFound);
        return;
    }

    uint32_t now = furi_get_tick();
    if((now - app->dwell_start_tick) >= FREQ_SCANNER_DWELL_MS) {
        app->freq_index = (app->freq_index + 1) % count;
        uint32_t next_hz = table[app->freq_index].freq_hz;
        float    next_mhz = next_hz / 1000000.0f;

        if(app->antenna_mode == AntennaInternal) {
            furi_hal_subghz_idle();
            furi_hal_subghz_set_frequency_and_path(next_hz);
            furi_hal_subghz_rx();
        } else {
            cc1101_set_frequency(app, next_mhz);
            cc1101_enter_rx(app);
        }
        app->current_freq_mhz = next_mhz;
        app->dwell_start_tick  = now;
    }
}
