/* I437: unit cases for the embedded cover readers (plugins/shared/stream/id3.h APIC, flac_meta.h PICTURE). */
#include "../../plugins/shared/stream/flac_meta.h"

#include <stdio.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); g_fail++; } } while (0)

static void put_ss(uint8_t* p, size_t v) { p[0] = (v >> 21) & 0x7F; p[1] = (v >> 14) & 0x7F; p[2] = (v >> 7) & 0x7F; p[3] = v & 0x7F; }

/* APIC body: enc 0, "image/png", type, empty description, payload. */
static size_t apic(uint8_t* o, int type, const uint8_t* pay, size_t n) {
    size_t i = 0;
    o[i++] = 0;
    memcpy(o + i, "image/png", 9); i += 9;
    o[i++] = 0; o[i++] = (uint8_t)type; o[i++] = 0;
    memcpy(o + i, pay, n);
    return i + n;
}

/* Appends one frame; ver 3 = big-endian size, ver 4 = syncsafe. */
static size_t frame(uint8_t* o, int ver, uint8_t flag2, const uint8_t* body, size_t n) {
    memcpy(o, "APIC", 4);
    if (ver == 4) put_ss(o + 4, n); else { o[4] = n >> 24; o[5] = n >> 16; o[6] = n >> 8; o[7] = n; }
    o[8] = 0; o[9] = flag2;
    memcpy(o + 10, body, n);
    return 10 + n;
}

static size_t header(uint8_t* o, int ver, uint8_t flags, size_t tag) {
    memcpy(o, "ID3", 3); o[3] = (uint8_t)ver; o[4] = 0; o[5] = flags; put_ss(o + 6, tag);
    return 10;
}

static void test_id3(void) {
    uint8_t t[512], b[128], work[512];
    const uint8_t imgA[4] = {1, 2, 3, 4}, imgB[3] = {9, 8, 7};
    size_t n, f1, f2;
    sel_cover c;

    /* v2.3: other-type picture first, front cover second: front wins */
    n = header(t, 3, 0, 0);
    f1 = apic(b, 0, imgA, 4); n += frame(t + n, 3, 0, b, f1);
    f2 = apic(b, 3, imgB, 3); n += frame(t + n, 3, 0, b, f2);
    put_ss(t + 6, n - 10);
    CHECK(sel_id3_cover(t, n, NULL, 0, 64, &c) && c.len == 3 && c.type == 3 && c.data[0] == 9 && strcmp(c.mime, "image/png") == 0);
    /* only non-front pictures: the first stays */
    n = header(t, 4, 0, 0);
    f1 = apic(b, 5, imgA, 4); n += frame(t + n, 4, 0, b, f1);
    f2 = apic(b, 0, imgB, 3); n += frame(t + n, 4, 0, b, f2);
    put_ss(t + 6, n - 10);
    CHECK(sel_id3_cover(t, n, NULL, 0, 64, &c) && c.len == 4 && c.type == 5);
    /* picture over the bound is skipped */
    CHECK(!sel_id3_cover(t, n, NULL, 0, 3, &c) || c.len == 3);
    /* truncated frame rejected */
    CHECK(!sel_id3_cover(t, n - 2, NULL, 0, 64, &c) || c.len == 4);
    n = header(t, 3, 0, 0);
    f1 = apic(b, 3, imgA, 4); n += frame(t + n, 3, 0, b, f1);
    put_ss(t + 6, n - 10);
    CHECK(!sel_id3_cover(t, n - 3, NULL, 0, 64, &c));
    /* syncsafe size: tag size field 0x00 0x00 0x02 0x01 = 257 */
    {
        uint8_t h[10]; header(h, 3, 0, 257);
        CHECK(sel_id3_syncsafe(h + 6) == 257u);
    }
    /* v2.4 frame unsynchronisation: payload FF 00 E0 decodes to FF E0; needs a work buffer */
    {
        const uint8_t raw[3] = {0xFF, 0xE0, 0x11};
        uint8_t uns[64];
        n = header(t, 4, 0, 0);
        f1 = apic(b, 3, raw, 3);
        /* build unsynchronised body by hand: insert 00 after the FF in the payload */
        memcpy(uns, b, f1 - 3);
        uns[f1 - 3] = 0xFF; uns[f1 - 2] = 0x00; uns[f1 - 1] = 0xE0; uns[f1] = 0x11;
        n += frame(t + n, 4, 0x02, uns, f1 + 1);
        put_ss(t + 6, n - 10);
        CHECK(!sel_id3_cover(t, n, NULL, 0, 64, &c) && c.work_need == f1 + 1);
        CHECK(sel_id3_cover(t, n, work, sizeof work, 64, &c) && c.len == 3 && c.data[0] == 0xFF && c.data[1] == 0xE0 && c.data[2] == 0x11);
    }
    /* a later unsynchronised frame must not overwrite the front cover already decoded into the work buffer */
    {
        const uint8_t raw[3] = {0xFF, 0xE0, 0x11};
        uint8_t uns[64];
        n = header(t, 4, 0, 0);
        f1 = apic(b, 3, raw, 3);
        memcpy(uns, b, f1 - 3);
        uns[f1 - 3] = 0xFF; uns[f1 - 2] = 0x00; uns[f1 - 1] = 0xE0; uns[f1] = 0x11;
        n += frame(t + n, 4, 0x02, uns, f1 + 1);
        f1 = apic(b, 4, raw, 3);
        memcpy(uns, b, f1 - 3);
        uns[f1 - 3] = 0xFF; uns[f1 - 2] = 0x00; uns[f1 - 1] = 0xE0; uns[f1] = 0x22;
        n += frame(t + n, 4, 0x02, uns, f1 + 1);
        put_ss(t + 6, n - 10);
        CHECK(sel_id3_cover(t, n, work, sizeof work, 64, &c) && c.type == 3 && c.len == 3 && c.data[2] == 0x11);
    }
    /* v2.3 whole-tag unsynchronisation flag */
    {
        const uint8_t raw[2] = {0xFF, 0xE0};
        uint8_t body[16], tag[64];
        size_t k = 0, bn;
        bn = apic(body, 3, raw, 2);
        /* frame header + body, then insert 00 after the FF payload byte */
        n = header(t, 3, 0x80, 0);
        k = frame(tag, 3, 0, body, bn);
        memcpy(t + n, tag, k - 2); n += k - 2;
        t[n++] = 0xFF; t[n++] = 0x00; t[n++] = 0xE0;
        put_ss(t + 6, n - 10);
        CHECK(sel_id3_cover(t, n, work, sizeof work, 64, &c) && c.len == 2 && c.data[0] == 0xFF && c.data[1] == 0xE0);
    }
    CHECK(!sel_id3_cover((const uint8_t*)"nope", 4, NULL, 0, 64, &c));
}

static void be(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void test_flac(void) {
    uint8_t f[256], pic[64];
    size_t n = 0, p = 0;
    sel_cover c;
    char album[32], artist[32];

    be(pic, 3); be(pic + 4, 9); memcpy(pic + 8, "image/png", 9); p = 17;
    be(pic + p, 0); p += 4; be(pic + p, 200); be(pic + p + 4, 200); be(pic + p + 8, 24); be(pic + p + 12, 0); p += 16;
    be(pic + p, 3); p += 4; pic[p++] = 5; pic[p++] = 6; pic[p++] = 7;
    memcpy(f, "fLaC", 4); n = 4;
    /* vorbis comment block: vendor "v", 2 comments */
    {
        uint8_t v[64]; size_t k = 0;
        v[k++] = 1; v[k++] = 0; v[k++] = 0; v[k++] = 0; v[k++] = 'v';
        v[k++] = 2; v[k++] = 0; v[k++] = 0; v[k++] = 0;
        v[k++] = 7; v[k++] = 0; v[k++] = 0; v[k++] = 0; memcpy(v + k, "album=X", 7); k += 7;
        v[k++] = 8; v[k++] = 0; v[k++] = 0; v[k++] = 0; memcpy(v + k, "ARTIST=Y", 8); k += 8;
        f[n] = 4; f[n + 1] = 0; f[n + 2] = 0; f[n + 3] = (uint8_t)k; memcpy(f + n + 4, v, k); n += 4 + k;
    }
    f[n] = 0x80 | 6; f[n + 1] = 0; f[n + 2] = 0; f[n + 3] = (uint8_t)p; memcpy(f + n + 4, pic, p); n += 4 + p;
    CHECK(sel_flac_meta(f, n, 64, &c, album, artist, sizeof album) && c.len == 3 && c.type == 3 && c.data[0] == 5);
    CHECK(strcmp(album, "X") == 0 && strcmp(artist, "Y") == 0 && strcmp(c.mime, "image/png") == 0);
    CHECK(!sel_flac_meta(f, n - 1, 64, &c, album, artist, sizeof album)); /* truncated picture block */
    CHECK(!sel_flac_meta(f, n, 2, &c, album, artist, sizeof album));      /* over the bound */
}

int main(void) {
    test_id3();
    test_flac();
    if (g_fail == 0) puts("cover_tag_ok");
    return g_fail ? 1 : 0;
}
