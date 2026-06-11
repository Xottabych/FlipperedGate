#include "signal_capture.h"
#include <furi_hal_subghz.h>

/* ── GDO0 interrupt handler (external CC1101) ───────────────────────────── */

void signal_capture_gdo0_cb(void* context) {
    AppState* app = context;

    if(!app->capturing) return;
    if(app->capture_done_flag) return;

    uint32_t now_cycles = DWT->CYCCNT;
    uint32_t cpu_mhz    = furi_hal_cortex_instructions_per_microsecond();

    bool     level        = furi_hal_gpio_read(app->pin_gd0);
    uint32_t elapsed_us   = (now_cycles - app->last_edge_tick) / cpu_mhz;
    app->last_edge_tick   = now_cycles;

    if(elapsed_us < CAPTURE_MIN_PULSE_US) return;

    if(elapsed_us > CAPTURE_GAP_TIMEOUT_US && app->capture_len > 0) {
        app->capturing = false;
        __atomic_store_n(&app->capture_done_flag, true, __ATOMIC_RELEASE);
        return;
    }

    if(app->capture_len >= CAPTURE_BUFFER_SIZE) {
        app->capturing = false;
        __atomic_store_n(&app->capture_done_flag, true, __ATOMIC_RELEASE);
        return;
    }

    app->capture_buf[app->capture_len++] =
        level ? (int32_t)elapsed_us : -(int32_t)elapsed_us;
}

/* ── Async RX callback (internal CC1101 via furi_hal_subghz) ────────────── */

static void internal_capture_cb(bool level, uint32_t duration, void* context) {
    AppState* app = context;

    if(!app->capturing || app->capture_done_flag) return;
    if(duration < CAPTURE_MIN_PULSE_US) return;

    if(duration > CAPTURE_GAP_TIMEOUT_US && app->capture_len > 0) {
        app->capturing = false;
        __atomic_store_n(&app->capture_done_flag, true, __ATOMIC_RELEASE);
        return;
    }

    if(app->capture_len >= CAPTURE_BUFFER_SIZE) {
        app->capturing = false;
        __atomic_store_n(&app->capture_done_flag, true, __ATOMIC_RELEASE);
        return;
    }

    /* level=true: signal present (mark); level=false: silence (space) */
    app->capture_buf[app->capture_len++] =
        level ? (int32_t)duration : -(int32_t)duration;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void signal_capture_start(AppState* app) {
    app->capture_len       = 0;
    app->capture_done_flag = false;
    app->capturing         = true;

    if(app->antenna_mode == AntennaInternal) {
        /* Radio is already in RX mode from the scanner */
        app->capture_rx_started = true;
        furi_hal_subghz_start_async_rx(internal_capture_cb, app);
    } else {
        app->last_edge_tick = DWT->CYCCNT;
        furi_hal_gpio_init(
            app->pin_gd0, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedVeryHigh);
        furi_hal_gpio_add_int_callback(app->pin_gd0, signal_capture_gdo0_cb, app);
        furi_hal_gpio_enable_int_callback(app->pin_gd0);
        app->gdo0_int_registered = true;
    }
}

void signal_capture_stop(AppState* app) {
    app->capturing = false;

    if(app->antenna_mode == AntennaInternal) {
        /* Guard: stop_async_rx furi_check-fails if RX was never started */
        if(app->capture_rx_started) {
            furi_hal_subghz_stop_async_rx();
            app->capture_rx_started = false;
        }
    } else {
        /* Guard: remove_int_callback on a pin that never had a callback
         * registered faults. The GDO0 interrupt is only installed when an
         * external capture actually starts, so a launch→exit without scanning
         * must NOT touch it. */
        if(app->gdo0_int_registered) {
            furi_hal_gpio_disable_int_callback(app->pin_gd0);
            furi_hal_gpio_remove_int_callback(app->pin_gd0);
            furi_hal_gpio_init(app->pin_gd0, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);
            app->gdo0_int_registered = false;
        }
    }
}

bool signal_capture_is_valid(AppState* app) {
    return app->capture_len >=
           (uint16_t)(app->squelch_threshold > 0 ? app->squelch_threshold : 10);
}

uint32_t signal_capture_compute_hash(AppState* app) {
    uint32_t hash  = 2166136261UL;
    const uint8_t* bytes = (const uint8_t*)app->capture_buf;
    size_t len = app->capture_len * sizeof(int32_t);
    for(size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

bool signal_capture_is_duplicate(AppState* app) {
    uint32_t hash = signal_capture_compute_hash(app);

    if(hash == app->last_capture_hash) {
        if((furi_get_tick() - app->last_capture_ts) < 2000) return true;
    }

    app->last_capture_hash = hash;
    app->last_capture_ts   = furi_get_tick();
    return false;
}
