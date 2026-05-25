#pragma once

#include "../subghz_gate_scanner.h"

#define CAPTURE_MIN_PULSE_US   150U
#define CAPTURE_GAP_TIMEOUT_US 35000U

void signal_capture_start(AppState* app);
void signal_capture_stop(AppState* app);
bool signal_capture_is_valid(AppState* app); /* pulse_count >= squelch_threshold */
bool signal_capture_is_duplicate(AppState* app);
uint32_t signal_capture_compute_hash(AppState* app);

/* GDO0 GPIO interrupt callback (registered in signal_capture_start) */
void signal_capture_gdo0_cb(void* context);
