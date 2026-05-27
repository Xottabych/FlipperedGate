#include "sub_file_parser.h"
#include <storage/storage.h>

bool sub_file_parse(AppState* app, const char* path) {
    app->capture_len = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    bool ok = false;

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    /* Read the file line by line */
    char     line[256];
    uint16_t pos   = 0;
    uint32_t freq  = 433920000;
    bool     in_data = false;

    while(true) {
        char c;
        uint16_t read = storage_file_read(file, &c, 1);
        if(read == 0) {
            /* EOF — flush last line */
            if(pos > 0) {
                line[pos] = '\0';
                /* fall through to process */
            } else {
                break;
            }
            c = '\n';
        }

        if(c != '\n' && c != '\r') {
            if(pos < (uint16_t)(sizeof(line) - 1)) line[pos++] = c;
            if(read == 0) break;
            continue;
        }

        if(read == 0 && pos == 0) break;

        line[pos] = '\0';
        pos = 0;

        if(strncmp(line, "Frequency:", 10) == 0) {
            freq = (uint32_t)atol(line + 10);
        } else if(strncmp(line, "RAW_Data:", 9) == 0) {
            in_data = true;
            const char* s = line + 9;
            while(*s && app->capture_len < CAPTURE_BUFFER_SIZE) {
                while(*s == ' ' || *s == '\t') s++;
                if(!*s) break;
                bool neg = (*s == '-');
                if(neg) s++;
                if(*s < '0' || *s > '9') break;
                int32_t v = 0;
                while(*s >= '0' && *s <= '9') {
                    v = v * 10 + (int32_t)(*s - '0');
                    s++;
                }
                app->capture_buf[app->capture_len++] = neg ? -v : v;
            }
        }

        if(read == 0) break;
    }

    (void)in_data;

    ok = (app->capture_len > 0);
    if(ok) {
        app->capture_freq_hz  = freq;
        app->current_freq_mhz = (float)freq / 1000000.0f;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
