#pragma once

/* KeeLoq learning type constants (from Unleashed / Flipper firmware) */
#define KL_SIMPLE      0  /* remote key = manufacturer seed directly */
#define KL_NORMAL      1  /* remote key derived via normal_learning(serial, seed) */
#define KL_MAGIC_XOR1  2  /* remote key = (serial|serial<<32) XOR seed */
#define KL_MAGIC_SER1  3  /* remote key = magic_serial_type1(serial, seed) */

typedef struct {
    const char* name;
    uint64_t    seed;
    uint8_t     type; /* one of KL_* constants above */
} KLMfrEntry;

/* ── Publicly documented / research-grade manufacturer seeds ─────────────── */
/*
 * Sources:
 *   - Microchip AN665 (HCS101 evaluation / demo key)
 *   - Security research papers (Bard/Oswald/Paar 2007-2008)
 *   - Flipper Zero community (DarkFlippers/unleashed-firmware)
 *
 * To add more keys: obtain the manufacturer seed (64-bit hex) and learning
 * type from a trusted source, then append an entry below.
 * The Unleashed firmware keystore at /ext/subghz/assets/keeloq_mfcodes
 * contains the full table but is AES-encrypted for legal reasons.
 */
static const KLMfrEntry kl_keys[] = {
    /* Microchip HCS101 / HCS200 evaluation chip — all-zeros demo key (AN665) */
    { "HCS-Demo", 0x0000000000000000ULL, KL_SIMPLE },

    /* Add more entries here as seeds become available, e.g.:
     * { "Doorhan",   0xYYYYYYYYYYYYYYYYULL, KL_NORMAL },
     * { "BFT Mitto", 0xYYYYYYYYYYYYYYYYULL, KL_NORMAL },
     */
};

static const uint8_t kl_keys_count =
    (uint8_t)(sizeof(kl_keys) / sizeof(kl_keys[0]));
