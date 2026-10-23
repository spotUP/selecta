#include "export.h"

#include <string.h>

static void put_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_le16(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

int sel_export_wav(const sel_rp_sample* s, sel_export_sink sink, void* ctx) {
    uint8_t h[44];
    uint32_t bytes_per = s->bits == 16u ? 2u : 1u;
    uint32_t data = s->length * bytes_per;
    uint32_t i;
    uint8_t buf[512];
    uint32_t fill = 0;
    if (s->data == NULL || s->length == 0u) {
        return 0;
    }
    memcpy(h, "RIFF", 4);
    put_le32(h + 4, 36u + data);
    memcpy(h + 8, "WAVEfmt ", 8);
    put_le32(h + 16, 16u);
    put_le16(h + 20, 1u);
    put_le16(h + 22, 1u);
    put_le32(h + 24, s->rate);
    put_le32(h + 28, s->rate * bytes_per);
    put_le16(h + 32, bytes_per);
    put_le16(h + 34, bytes_per * 8u);
    memcpy(h + 36, "data", 4);
    put_le32(h + 40, data);
    if (!sink(ctx, h, sizeof h)) {
        return 0;
    }
    for (i = 0; i < s->length; i++) {
        if (fill + 2u > sizeof buf) {
            if (!sink(ctx, buf, fill)) {
                return 0;
            }
            fill = 0;
        }
        if (bytes_per == 2u) {
            uint16_t v = (uint16_t)(((const int16_t*)s->data)[i]);
            buf[fill++] = (uint8_t)v;
            buf[fill++] = (uint8_t)(v >> 8);
        } else {
            buf[fill++] = (uint8_t)(((const int8_t*)s->data)[i] + 128);
        }
    }
    return fill == 0u || sink(ctx, buf, fill);
}
