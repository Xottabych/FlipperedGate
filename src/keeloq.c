#include "keeloq.h"
#include "keeloq_keys.h"

/* ── KeeLoq NLF and bit helpers ──────────────────────────────────────────── */

#define KEELOQ_NLF    0x3A5C742EUL
#define KL_BIT(x, n)  (((uint32_t)(x) >> (n)) & 1u)
#define KL_G5(x, a, b, c, d, e) \
    (KL_BIT(x,a) | (KL_BIT(x,b)<<1) | (KL_BIT(x,c)<<2) | \
     (KL_BIT(x,d)<<3) | (KL_BIT(x,e)<<4))

/* ── Core decrypt (Unleashed / Microchip HCS300 NLFSR, 528 rounds) ───────── */

static uint32_t kl_decrypt(uint32_t data, uint64_t key) {
    uint32_t x = data, r;
    for(r = 0; r < 528; r++)
        x = (x << 1) ^ KL_BIT(x, 31) ^ KL_BIT(x, 15) ^
            (uint32_t)KL_BIT(key, (15 - r) & 63) ^
            KL_BIT(KEELOQ_NLF, KL_G5(x, 0, 8, 19, 25, 30));
    return x;
}

/* ── Learning algorithms ─────────────────────────────────────────────────── */

static uint64_t kl_learn_normal(uint32_t serial, uint64_t seed) {
    uint32_t k1, k2, data;
    data = (serial & 0x0FFFFFFF) | 0x20000000u;
    k1   = kl_decrypt(data, seed);
    data = (serial & 0x0FFFFFFF) | 0x60000000u;
    k2   = kl_decrypt(data, seed);
    return ((uint64_t)k2 << 32) | k1;
}

static uint64_t kl_learn_magic_xor1(uint32_t serial, uint64_t seed) {
    uint32_t s = serial & 0x0FFFFFFF;
    return (((uint64_t)s << 32) | s) ^ seed;
}

static uint64_t kl_learn_magic_serial1(uint32_t serial, uint64_t seed) {
    return (seed & 0xFFFFFFFF) |
           ((uint64_t)serial << 40) |
           ((uint64_t)(((serial & 0xFF) + ((serial >> 8) & 0xFF)) & 0xFF) << 32);
}

/* ── Verification ────────────────────────────────────────────────────────── */

static bool kl_verify(uint32_t hop, uint32_t fix, uint64_t remote_key,
                      uint8_t* btn_out, uint16_t* cnt_out) {
    uint32_t plain    = kl_decrypt(hop, remote_key);
    uint8_t  p_btn    = (uint8_t)((plain >> 28) & 0xF);
    uint8_t  p_ser8   = (uint8_t)(plain & 0xFF);
    uint8_t  f_btn    = (uint8_t)((fix   >> 28) & 0xF);
    uint8_t  f_ser8   = (uint8_t)(fix   & 0xFF);
    if(p_btn != f_btn || p_ser8 != f_ser8) return false;
    if(btn_out) *btn_out = p_btn;
    if(cnt_out) *cnt_out = (uint16_t)((plain >> 16) & 0xFFF);
    return true;
}

/* ── Bit extraction from timing buffer ──────────────────────────────────── */

#define KL_MAX_BITS 80
#define KL_TE_MIN_US 200   /* minimum Te in μs */
#define KL_TE_MAX_US 800   /* maximum Te in μs */

static bool kl_extract_bits(const int32_t* buf, uint16_t len,
                             uint8_t* bits, uint8_t* nbits_out) {
    /* Estimate Te = average width of positive marks within plausible range */
    uint32_t te_sum = 0;
    uint16_t te_cnt = 0;
    for(uint16_t i = 0; i < len && te_cnt < 32; i++) {
        if(buf[i] > 0) {
            uint32_t d = (uint32_t)buf[i];
            if(d >= KL_TE_MIN_US && d <= KL_TE_MAX_US) {
                te_sum += d;
                te_cnt++;
            }
        }
    }
    if(te_cnt < 4) return false;
    uint32_t te = te_sum / te_cnt;

    /* Find preamble: at least 12 consecutive (mark≈Te, space≈Te) pairs */
    uint16_t preamble_end = 0;
    uint16_t run = 0;
    for(uint16_t i = 0; i + 1 < len; i += 2) {
        uint32_t m = (buf[i]   > 0) ? (uint32_t)buf[i]   : 0u;
        uint32_t s = (buf[i+1] < 0) ? (uint32_t)-buf[i+1] : 0u;
        bool equal_pair = (m >= te / 2 && m <= te * 2 &&
                           s >= te / 2 && s <= te * 2);
        if(equal_pair) {
            run++;
        } else {
            if(run >= 12) {
                preamble_end = i;
                break;
            }
            run = 0;
        }
    }
    if(preamble_end == 0 && run >= 12) {
        /* preamble at end — no data bits follow, frame not found */
        return false;
    }
    if(preamble_end == 0) return false;

    /* Extract data bits after preamble: mark≈Te, space≈Te→0, space≈3Te→1 */
    uint8_t n = 0;
    for(uint16_t i = preamble_end; i + 1 < len && n < KL_MAX_BITS; i += 2) {
        uint32_t m = (buf[i]   > 0) ? (uint32_t)buf[i]   : 0u;
        uint32_t s = (buf[i+1] < 0) ? (uint32_t)-buf[i+1] : 0u;
        if(m < te / 2 || m > te * 2) break;
        if(s >= te / 2 && s <= te * 2) {
            bits[n++] = 0;
        } else if(s >= te * 2 && s <= te * 5) {
            bits[n++] = 1;
        } else {
            break; /* invalid space width */
        }
    }
    *nbits_out = n;
    return n >= 66;
}

/* ── Frame parsing (HCS300/HCS301, LSB first) ────────────────────────────── */

static bool kl_parse_frame(const uint8_t* bits, uint8_t nbits,
                            uint32_t* hop_out, uint32_t* fix_out) {
    if(nbits < 66) return false;
    uint32_t hop = 0, fix = 0;
    for(uint8_t i = 0; i < 32; i++) hop |= ((uint32_t)bits[i]      << i);
    for(uint8_t i = 0; i < 32; i++) fix |= ((uint32_t)bits[32 + i] << i);
    *hop_out = hop;
    *fix_out = fix;
    return true;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

bool keeloq_try_decode(const int32_t* buf, uint16_t len, KeeLoqResult* out) {
    out->decoded  = false;
    out->mfr_name = NULL;

    uint8_t bits[KL_MAX_BITS];
    uint8_t nbits = 0;
    if(!kl_extract_bits(buf, len, bits, &nbits)) return false;

    uint32_t hop, fix;
    if(!kl_parse_frame(bits, nbits, &hop, &fix)) return false;

    out->hop    = hop;
    out->fix    = fix;
    out->serial = fix & 0x0FFFFFFFu;
    out->btn    = (uint8_t)((fix >> 28) & 0xF);

    for(uint8_t k = 0; k < kl_keys_count; k++) {
        uint64_t remote_key;
        switch(kl_keys[k].type) {
        case KL_SIMPLE:
            remote_key = kl_keys[k].seed;
            break;
        case KL_NORMAL:
            remote_key = kl_learn_normal(out->serial, kl_keys[k].seed);
            break;
        case KL_MAGIC_XOR1:
            remote_key = kl_learn_magic_xor1(out->serial, kl_keys[k].seed);
            break;
        case KL_MAGIC_SER1:
            remote_key = kl_learn_magic_serial1(out->serial, kl_keys[k].seed);
            break;
        default:
            continue;
        }
        uint8_t btn; uint16_t cnt;
        if(kl_verify(hop, fix, remote_key, &btn, &cnt)) {
            out->decoded  = true;
            out->mfr_name = kl_keys[k].name;
            out->btn      = btn;
            out->cnt      = cnt;
            return true;
        }
    }
    return false; /* frame extracted OK but no key matched */
}
