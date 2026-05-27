#pragma once
#include "../subghz_gate_scanner.h"

/* Read a .sub file and fill app->capture_buf, capture_len,
 * capture_freq_hz, current_freq_mhz.
 * Returns false if the file cannot be opened or contains no RAW_Data. */
bool sub_file_parse(AppState* app, const char* path);
