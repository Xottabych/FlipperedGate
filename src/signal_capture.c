#include "signal_capture.h"

/* ── GDO0 interrupt handler ─────────────────────────────────────────────── */

void signal_capture_gdo0_cb(void* context) {
    AppState* app = context;

    if(!app->capturing) return;
    if(app->capture_done_flag) return;

    /* Use DWT cycle counter for microsecond-precision timing.
       The Flipper firmware enables DWT on boot. */
    uint32_t now_cycles = DWT->CYCCNT;
    uint32_t cpu_mhz = furi_hal_cortex_instructions_per_microsecond();

    bool level = furi_hal_gpio_read(app->pin_gd0);
    uint32_t elapsed_cycles = now_cycles - app->last_edge_tick;
    uint32_t elapsed_us     = elapsed_cycles / cpu_mhz;
    app->last_edge_tick = now_cycles;

    /* Ignore glitches shorter than minimum pulse */
    if(elapsed_us < CAPTURE_MIN_PULSE_US) return;

    /* Gap timeout: treat as end-of-frame */
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

    /* Positive = mark (signal present), negative = space */
    app->capture_buf[app->capture_len++] =
        level ? (int32_t)elapsed_us : -(int32_t)elapsed_us;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void signal_capture_start(AppState* app) {
    app->capture_len       = 0;
    app->capture_done_flag = false;
    app->capturing         = true;
    app->last_edge_tick    = DWT->CYCCNT;

    furi_hal_gpio_init(
        app->pin_gd0, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_add_int_callback(app->pin_gd0, signal_capture_gdo0_cb, app);
    furi_hal_gpio_enable_int_callback(app->pin_gd0);
}

void signal_capture_stop(AppState* app) {
    app->capturing = false;
    furi_hal_gpio_disable_int_callback(app->pin_gd0);
    furi_hal_gpio_remove_int_callback(app->pin_gd0);
    furi_hal_gpio_init(app->pin_gd0, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);
}

bool signal_capture_is_valid(AppState* app) {
    /* squelch_threshold reused as minimum pulse count */
    return app->capture_len >= (uint16_t)(app->squelch_threshold > 0 ? app->squelch_threshold : 10);
}

uint32_t signal_capture_compute_hash(AppState* app) {
    uint32_t hash = 2166136261UL;
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
        uint32_t elapsed = furi_get_tick() - app->last_capture_ts;
        if(elapsed < 2000) {
            return true;
        }
    }

    app->last_capture_hash = hash;
    app->last_capture_ts   = furi_get_tick();
    return false;
}
