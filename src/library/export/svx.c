#include "export.h"

#include <string.h>

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

int sel_export_svx(const sel_rp_sample* s, sel_export_sink sink, void* ctx) {
    uint8_t h[64];
    uint32_t i;
    uint32_t pad = s->length & 1u;
    uint8_t buf[512];
    uint32_t fill = 0;
    uint32_t one_shot, repeat;
    if (s->data == NULL || s->length == 0u) {
        return 0;
    }
    if (s->loop_length > 1u) {
        one_shot = s->loop_start;
        repeat = s->loop_length;
    } else {
        one_shot = s->length;
        repeat = 0u;
    }
    memcpy(h, "FORM", 4);
    put_be32(h + 4, 4u + 8u + 20u + 8u + s->length + pad);
    memcpy(h + 8, "8SVXVHDR", 8);
    put_be32(h + 16, 20u);
    put_be32(h + 20, one_shot);
    put_be32(h + 24, repeat);
    put_be32(h + 28, 0u); /* samplesPerHiCycle */
    h[32] = (uint8_t)(s->rate >> 8);
    h[33] = (uint8_t)s->rate;
    h[34] = 1u; /* octaves */
    h[35] = 0u; /* sCompression: none */
    put_be32(h + 36, 0x10000u);
    memcpy(h + 40, "BODY", 4);
    put_be32(h + 44, s->length);
    if (!sink(ctx, h, 48u)) {
        return 0;
    }
    for (i = 0; i < s->length; i++) {
        if (fill + 1u > sizeof buf) {
            if (!sink(ctx, buf, fill)) {
                return 0;
            }
            fill = 0;
        }
        if (s->bits == 16u) {
            buf[fill++] = (uint8_t)((uint16_t)(((const int16_t*)s->data)[i]) >> 8);
        } else {
            buf[fill++] = ((const uint8_t*)s->data)[i];
        }
    }
    if (pad != 0u) {
        buf[fill++] = 0u;
    }
    return fill == 0u || sink(ctx, buf, fill);
}
