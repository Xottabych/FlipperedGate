#pragma once

#include "../subghz_gate_scanner.h"

#define FILE_MANAGER_SAVE_DIR    "/ext/subghz"
#define FILE_MANAGER_LINE_PULSES 512U

bool file_manager_save(AppState* app);

/* Internal helpers */
void file_manager_make_filename(float freq_mhz, uint32_t ts, char* buf, size_t len);
bool file_manager_write_header(File* file, uint32_t freq_hz);
bool file_manager_write_raw_data(File* file, const int32_t* pulses, uint16_t count);
