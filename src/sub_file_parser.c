#include "sub_file_parser.h"
#include <storage/storage.h>

bool sub_file_parse(AppState* app, const char* path) {
    app->capture_len = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint32_t freq    = 433920000;
    char     hdr[128];
    uint16_t hdr_pos = 0;

    /* Two-phase parse: collect header key=value lines normally (they are short),
     * but switch to inline streaming mode as soon as "RAW_Data:" (9 chars) is
     * detected — this avoids buffering the full line which can be 4 KB+. */
    bool    in_raw    = false;
    bool    neg       = false;
    int32_t val       = 0;
    bool    has_digit = false;

    while(app->capture_len < CAPTURE_BUFFER_SIZE) {
        char     c;
        if(storage_file_read(file, &c, 1) == 0) {
            /* EOF — flush any pending number */
            if(in_raw && has_digit)
                app->capture_buf[app->capture_len++] = neg ? -val : val;
            break;
        }

        if(!in_raw) {
            if(c == '\n' || c == '\r') {
                hdr[hdr_pos] = '\0';
                if(strncmp(hdr, "Frequency:", 10) == 0)
                    freq = (uint32_t)atol(hdr + 10);
                hdr_pos = 0;
            } else {
                if(hdr_pos < (uint16_t)(sizeof(hdr) - 1))
                    hdr[hdr_pos++] = c;
                /* Switch to streaming mode the moment the 9-char prefix matches */
                if(hdr_pos == 9 && strncmp(hdr, "RAW_Data:", 9) == 0) {
                    in_raw    = true;
                    has_digit = false;
                    val       = 0;
                    neg       = false;
                    hdr_pos   = 0;
                }
            }
        } else {
            /* Streaming integer parse — never buffers the full line */
            if(c == '\n' || c == '\r') {
                if(has_digit && app->capture_len < CAPTURE_BUFFER_SIZE)
                    app->capture_buf[app->capture_len++] = neg ? -val : val;
                in_raw    = false;
                has_digit = false;
                val       = 0;
                neg       = false;
                hdr_pos   = 0;
            } else if(c == '-') {
                if(!has_digit) neg = true;
            } else if(c >= '0' && c <= '9') {
                val       = val * 10 + (int32_t)(c - '0');
                has_digit = true;
            } else if(c == ' ' || c == '\t') {
                if(has_digit && app->capture_len < CAPTURE_BUFFER_SIZE) {
                    app->capture_buf[app->capture_len++] = neg ? -val : val;
                    has_digit = false;
                    val       = 0;
                    neg       = false;
                }
            }
        }
    }

    bool ok = (app->capture_len > 0);
    if(ok) {
        app->capture_freq_hz  = freq;
        app->current_freq_mhz = (float)freq / 1000000.0f;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
