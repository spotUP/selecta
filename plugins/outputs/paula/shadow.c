/* paula-shadow output (plan M2 phase 3, I120): the Paula voice sink of
 * paula_voices.c over a logging register bus instead of the chip. The same
 * sink code runs under vamos and on the desktop, so T51 compares one SHA-256
 * of the register log across both trees. Chip addresses are not logged (they
 * differ per host); the copy serial and the byte offset are. Integer only. */
#include <selecta/output.h>
#include <selecta/voices.h>

#include "paula_out.h"

#define SHADOW_LOG_LEVEL SEL_LOG_INFO /* config paula.log = 1 echoes each line through host->log */

/* ---- SHA-256 (FIPS 180-4), streaming, integer only ---- */
typedef struct sel_sha256 {
    uint32_t h[8];
    uint8_t buf[64];
    uint32_t fill;
    uint32_t total;
} sel_sha256;

static const uint32_t k256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32u - (n))))

static void sha_block(sel_sha256* s, const uint8_t* p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2, i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) |
               (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + k256[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(sel_sha256* s) {
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint32_t i;
    for (i = 0; i < 8; i++) {
        s->h[i] = iv[i];
    }
    s->fill = 0;
    s->total = 0;
}

static void sha_update(sel_sha256* s, const uint8_t* p, uint32_t n) {
    s->total += n;
    while (n > 0) {
        s->buf[s->fill++] = *p++;
        n--;
        if (s->fill == 64) {
            sha_block(s, s->buf);
            s->fill = 0;
        }
    }
}

/* Digest of a copy, so the log can go on after a stats read. */
static void sha_hex(const sel_sha256* in, char* out) {
    static const char hex[] = "0123456789abcdef";
    sel_sha256 s = *in;
    uint32_t bits_hi = s.total >> 29, bits_lo = s.total << 3, i;
    uint8_t pad = 0x80;
    sha_update(&s, &pad, 1);
    pad = 0;
    while (s.fill != 56) {
        sha_update(&s, &pad, 1);
    }
    for (i = 0; i < 4; i++) {
        uint8_t b = (uint8_t)(bits_hi >> (24 - 8 * i));
        sha_update(&s, &b, 1);
    }
    for (i = 0; i < 4; i++) {
        uint8_t b = (uint8_t)(bits_lo >> (24 - 8 * i));
        sha_update(&s, &b, 1);
    }
    for (i = 0; i < 32; i++) {
        uint8_t b = (uint8_t)(s.h[i / 4] >> (24 - 8 * (i % 4)));
        out[i * 2] = hex[b >> 4];
        out[i * 2 + 1] = hex[b & 15];
    }
    out[64] = 0;
}

/* ---- the logging bus ---- */
typedef struct shadow {
    const sel_host* host;
    sel_sha256 sum;
    uint32_t aud_writes;
    int echo;
    char digest[65];
} shadow;

static char* put_hex(char* p, uint32_t v, uint32_t digits) {
    static const char hex[] = "0123456789abcdef";
    uint32_t i;
    for (i = 0; i < digits; i++) {
        *p++ = hex[(v >> (4 * (digits - 1 - i))) & 15u];
    }
    return p;
}

static void log_line(shadow* s, const char* tag, uint32_t a, uint32_t b, uint32_t c) {
    char line[48];
    char* p = line;
    while (*tag) {
        *p++ = *tag++;
    }
    *p++ = ' ';
    p = put_hex(p, a, 4);
    *p++ = ' ';
    p = put_hex(p, b, 8);
    if (c != 0xFFFFFFFFu) {
        *p++ = ' ';
        p = put_hex(p, c, 8);
    }
    *p++ = '\n';
    sha_update(&s->sum, (const uint8_t*)line, (uint32_t)(p - line));
    if (s->echo) {
        p[-1] = 0;
        s->host->log(s->host, SHADOW_LOG_LEVEL, line);
    }
}

static void bus_write(void* ctx, uint32_t reg, uint32_t value) {
    shadow* s = (shadow*)ctx;
    if (reg >= SEL_PAULA_AUD_LCH && reg < SEL_PAULA_AUD_LCH + SEL_PAULA_VOICES * SEL_PAULA_CHANNEL_STRIDE) {
        s->aud_writes++;
    }
    log_line(s, "W", reg, value & 0xFFFFu, 0xFFFFFFFFu);
}

static void bus_set_lc(void* ctx, uint32_t channel, const void* bytes, uint32_t offset, uint32_t serial) {
    shadow* s = (shadow*)ctx;
    (void)bytes;
    s->aud_writes++;
    log_line(s, "LC", SEL_PAULA_AUD_LCH + channel * SEL_PAULA_CHANNEL_STRIDE, serial, offset);
}

static void* shadow_bus_open(const sel_host* host, sel_paula_bus* bus) {
    shadow* s = (shadow*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    s->host = host;
    s->aud_writes = 0;
    s->echo = host->config_get != NULL && host->config_get(host, "paula.log", "0")[0] == '1';
    sha_init(&s->sum);
    bus->write = bus_write;
    bus->set_lc = bus_set_lc;
    bus->ctx = s;
    return s;
}

static void shadow_bus_close(const sel_host* host, void* ctx) {
    host->mem_free(host, ctx);
}

/* The digest is of a copy, so the log can go on after a stats read. */
static uint32_t shadow_bus_stats(void* ctx, sel_tag* out) {
    shadow* s = (shadow*)ctx;
    sha_hex(&s->sum, s->digest);
    out[0].tag = SEL_OUT_STAT_REG_WRITES;
    out[0].value = s->aud_writes;
    out[1].tag = SEL_OUT_STAT_LOG_DIGEST;
    out[1].value = (uintptr_t)s->digest;
    return 2;
}

static const sel_paula_out_kind g_kind = {"paula-shadow", "Paula shadow", 0, shadow_bus_open, shadow_bus_close,
                                          shadow_bus_stats};
static sel_tag g_hooks[16];
static sel_tag g_desc[5];

static void* shadow_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    return sel_paula_out_open(&g_kind, host, rate, channels);
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return sel_paula_out_describe(&g_kind, shadow_open, g_hooks, g_desc);
}
