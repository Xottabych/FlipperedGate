#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../subghz_gate_scanner.h"

typedef struct {
    /* Frame always extracted (no key needed) */
    uint32_t hop;     /* 32-bit hopping code (encrypted) */
    uint32_t fix;     /* 32-bit fixed code */
    uint32_t serial;  /* 28-bit serial number (plaintext) */
    uint8_t  btn;     /* 4-bit button code (plaintext) */
    /* Available only if decryption succeeded */
    bool     decoded;
    const char* mfr_name;
    uint16_t cnt;     /* 12-bit sync counter */
} KeeLoqResult;

/* Try to extract and decode a KeeLoq frame from a raw timing buffer.
 * out->decoded = true if a known manufacturer key matched.
 * Always fills hop/fix/serial/btn if the frame can be extracted.
 * Returns false if no KeeLoq frame could be found in the buffer. */
bool keeloq_try_decode(const int32_t* buf, uint16_t len, KeeLoqResult* out);
