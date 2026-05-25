#include "file_manager.h"

void file_manager_make_filename(float freq_mhz, uint32_t ts, char* buf, size_t len) {
    uint32_t mhz_int  = (uint32_t)freq_mhz;
    uint32_t mhz_frac = (uint32_t)((freq_mhz - (float)mhz_int) * 100.0f + 0.5f);
    snprintf(buf, len, "%s/Gate_%lu_%02lu_%lu.sub",
             FILE_MANAGER_SAVE_DIR, (unsigned long)mhz_int,
             (unsigned long)mhz_frac, (unsigned long)ts);
}

bool file_manager_write_header(File* file, uint32_t freq_hz) {
    char line[64];
    const char* hdr =
        "Filetype: Flipper SubGhz RAW File\n"
        "Version: 1\n";
    if(!storage_file_write(file, hdr, strlen(hdr))) return false;

    int n = snprintf(line, sizeof(line), "Frequency: %lu\n", (unsigned long)freq_hz);
    if(!storage_file_write(file, line, (uint16_t)n)) return false;

    const char* preset =
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n";
    return storage_file_write(file, preset, strlen(preset)) > 0;
}

bool file_manager_write_raw_data(File* file, const int32_t* pulses, uint16_t count) {
    if(count == 0) return false;

    char num[16];
    const char* prefix = "RAW_Data:";

    if(!storage_file_write(file, prefix, strlen(prefix))) return false;

    for(uint16_t i = 0; i < count; i++) {
        int n = snprintf(num, sizeof(num), " %ld", (long)pulses[i]);
        if(!storage_file_write(file, num, (uint16_t)n)) return false;

        /* Line wrap every FILE_MANAGER_LINE_PULSES values */
        if(((i + 1) % FILE_MANAGER_LINE_PULSES == 0) && (i + 1) < count) {
            if(!storage_file_write(file, "\nRAW_Data:", 10)) return false;
        }
    }
    return storage_file_write(file, "\n", 1) > 0;
}

bool file_manager_save(AppState* app) {
    if(app->capture_len == 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Ensure output directory exists */
    if(!storage_dir_exists(storage, FILE_MANAGER_SAVE_DIR)) {
        storage_simply_mkdir(storage, FILE_MANAGER_SAVE_DIR);
    }

    /* Build timestamped filename */
    uint32_t ts = furi_hal_rtc_get_timestamp();

    file_manager_make_filename(
        app->current_freq_mhz, ts, app->save_path, sizeof(app->save_path));

    /* Open file */
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, app->save_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) {
        FURI_LOG_E("FileManager", "Cannot open %s", app->save_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    ok = file_manager_write_header(file, app->capture_freq_hz)
      && file_manager_write_raw_data(file, app->capture_buf, app->capture_len);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        FURI_LOG_I("FileManager", "Saved %u pulses → %s",
                   app->capture_len, app->save_path);
    }
    return ok;
}
