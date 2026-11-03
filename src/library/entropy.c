/* Entropy pool; see entropy.h. */
#include "entropy.h"

#include <stdio.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
#define POOL_BACKEND 1
#else
#define POOL_BACKEND 0
#endif

/* Minimal SHA-256 (FIPS 180-4). */
typedef struct {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t len;
    size_t fill;
} sha256;

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(sha256* c, const uint8_t* p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h;
    unsigned i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));

        h = g, g = f, f = e, e = d + t1, d = cc, cc = b, b = a, a = t1 + t2;
    }
    c->h[0] += a, c->h[1] += b, c->h[2] += cc, c->h[3] += d, c->h[4] += e, c->h[5] += f, c->h[6] += g, c->h[7] += h;
}

static void sha_init(sha256* c) {
    static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    memcpy(c->h, iv, sizeof iv);
    c->len = 0;
    c->fill = 0;
}

static void sha_update(sha256* c, const void* data, size_t n) {
    const uint8_t* p = data;

    c->len += n;
    while (n > 0) {
        size_t take = 64 - c->fill < n ? 64 - c->fill : n;

        memcpy(c->buf + c->fill, p, take);
        c->fill += take, p += take, n -= take;
        if (c->fill == 64) {
            sha_block(c, c->buf);
            c->fill = 0;
        }
    }
}

static void sha_final(sha256* c, uint8_t out[32]) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80;
    uint8_t zero = 0;
    uint8_t lenb[8];
    unsigned i;

    sha_update(c, &pad, 1);
    while (c->fill != 56) {
        sha_update(c, &zero, 1);
    }
    for (i = 0; i < 8; i++) {
        lenb[i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    sha_update(c, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(c->h[i] >> 24), out[4 * i + 1] = (uint8_t)(c->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(c->h[i] >> 8), out[4 * i + 3] = (uint8_t)c->h[i];
    }
}

static uint8_t g_pool[32];
static unsigned g_samples;
static uint32_t g_out_counter;
static int g_mode; /* sel_entropy_test_mode */
static char g_seed_path[256];

static void be32(uint8_t out[4], uint32_t v) {
    out[0] = (uint8_t)(v >> 24), out[1] = (uint8_t)(v >> 16), out[2] = (uint8_t)(v >> 8), out[3] = (uint8_t)v;
}

static int use_pool(void) {
    return POOL_BACKEND || g_mode == 2;
}

void sel_entropy_add(uint32_t sample) {
    sha256 c;
    uint8_t s[8];

    be32(s, sample);
    be32(s + 4, g_samples);
    sha_init(&c);
    sha_update(&c, "add", 3);
    sha_update(&c, g_pool, sizeof g_pool);
    sha_update(&c, s, sizeof s);
    sha_final(&c, g_pool);
    g_samples++;
}

unsigned sel_entropy_samples(void) {
    return g_samples;
}

static int urandom_fill(void* buf, size_t n) {
    FILE* f = fopen("/dev/urandom", "rb");
    size_t got;

    if (f == NULL) {
        return -1;
    }
    got = fread(buf, 1, n, f);
    fclose(f);
    return got == n ? 0 : -1;
}

int sel_entropy_ready(void) {
    if (g_mode == 1) {
        return 0;
    }
    if (use_pool()) {
        return g_samples >= SEL_ENTROPY_THRESHOLD;
    }
    return 1;
}

int sel_entropy_bytes(void* buf, size_t n) {
    uint8_t* out = buf;
    sha256 c;

    if (!sel_entropy_ready()) {
        return -1;
    }
    if (!use_pool()) {
        return urandom_fill(buf, n);
    }
    while (n > 0) {
        uint8_t blk[32];
        uint8_t cnt[4];
        size_t take = n < 32 ? n : 32;

        be32(cnt, g_out_counter++);
        sha_init(&c);
        sha_update(&c, "out", 3);
        sha_update(&c, g_pool, sizeof g_pool);
        sha_update(&c, cnt, 4);
        sha_final(&c, blk);
        memcpy(out, blk, take);
        out += take, n -= take;
    }
    /* Ratchet: a later pool state cannot reveal what was handed out. */
    sha_init(&c);
    sha_update(&c, "ratchet", 7);
    sha_update(&c, g_pool, sizeof g_pool);
    sha_final(&c, g_pool);
    return 0;
}

int sel_entropy_seed_load(const char* path) {
    uint8_t seed[64];
    size_t got;
    FILE* f;
    sha256 c;

    if (strlen(path) >= sizeof g_seed_path) {
        return -1;
    }
    memcpy(g_seed_path, path, strlen(path) + 1);
    f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    got = fread(seed, 1, sizeof seed, f);
    fclose(f);
    sha_init(&c);
    sha_update(&c, "seed", 4);
    sha_update(&c, g_pool, sizeof g_pool);
    sha_update(&c, seed, got);
    sha_final(&c, g_pool);
    return 0;
}

int sel_entropy_seed_save(void) {
    uint8_t seed[32];
    FILE* f;
    size_t put;

    if (g_seed_path[0] == '\0' || sel_entropy_bytes(seed, sizeof seed) != 0) {
        return -1;
    }
    f = fopen(g_seed_path, "wb");
    if (f == NULL) {
        return -1;
    }
    put = fwrite(seed, 1, sizeof seed, f);
    return fclose(f) == 0 && put == sizeof seed ? 0 : -1;
}

void sel_entropy_test_mode(int mode) {
    g_mode = mode;
    memset(g_pool, 0, sizeof g_pool);
    g_samples = 0;
    g_out_counter = 0;
    g_seed_path[0] = '\0';
}
