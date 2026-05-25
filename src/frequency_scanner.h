#pragma once

#include "../subghz_gate_scanner.h"
#include "cc1101_ext.h"

#define FREQ_SCANNER_MAX_FREQS   16U
#define FREQ_SCANNER_DWELL_MS    200U

typedef enum {
    ScannerState_Idle,
    ScannerState_Hopping,
    ScannerState_Locked,
} ScannerState;

typedef struct {
    uint32_t freq_hz;
} FreqEntry;

typedef struct FrequencyScanner {
    CC1101Handle_unused* _unused; /* placeholder — scanner uses AppState directly */
    ScannerState state;
    uint8_t      freq_count;
    uint8_t      freq_index;
    int8_t       rssi_latest;
    uint32_t     detected_freq_hz;
    int8_t       detected_rssi_dbm;
} FrequencyScanner;

/* Frequency tables per band */
extern const FreqEntry freq_table_all[];
extern const uint8_t   freq_table_all_count;
extern const FreqEntry freq_table_433[];
extern const uint8_t   freq_table_433_count;
extern const FreqEntry freq_table_868[];
extern const uint8_t   freq_table_868_count;
extern const FreqEntry freq_table_315[];
extern const uint8_t   freq_table_315_count;

void freq_scanner_init(AppState* app);
void freq_scanner_start(AppState* app);
void freq_scanner_stop(AppState* app);
void freq_scanner_tick(void* context); /* FuriTimer callback */
