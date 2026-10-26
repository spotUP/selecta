/* PowerPacker 2.0 ("PP20") decrunch, written for Selecta (I159a). The format and the backward bit
 * order follow the public description and Teemu Suutari's `ancient` (BSD 2-Clause) as reference; no
 * code is copied.
 *
 * Layout: "PP20", 4 bytes of match offset widths, packed bits, then a 32-bit trailer holding the
 * unpacked size (24 bits) and the number of padding bits to skip (8 bits). The bit stream is read
 * backward from the trailer, least significant bit of each byte first; multi-bit values are assembled
 * most significant bit first. The output is filled from its end. */
#include "archive.h"

typedef struct pp_bits {
    const uint8_t* d;
    size_t pos; /* bytes before pos are unread */
    size_t lo;  /* lowest readable byte index */
    unsigned cur, left;
    int bad;
} pp_bits;

static unsigned pp_bit(pp_bits* b) {
    unsigned v;

    if (b->left == 0) {
        if (b->pos <= b->lo) {
            b->bad = 1;
            return 0;
        }
        b->cur = b->d[--b->pos];
        b->left = 8;
    }
    v = b->cur & 1u;
    b->cur >>= 1;
    b->left--;
    return v;
}

static unsigned pp_bits_n(pp_bits* b, unsigned n) {
    unsigned v = 0;

    while (n--) {
        v = (v << 1) | pp_bit(b);
    }
    return v;
}

uint8_t* sel_pp20_depack(const sel_host* host, const uint8_t* data, size_t size, size_t max_out,
                         size_t* out_size) {
    static const uint8_t ok_tables[5][4] = {
        {9, 9, 9, 9}, {9, 10, 10, 10}, {9, 10, 11, 11}, {9, 10, 12, 12}, {9, 10, 12, 13}};
    pp_bits b;
    uint8_t* out;
    size_t raw, pos;
    unsigned skip, i;
    int t;

    if (size < 16 || data[0] != 'P' || data[1] != 'P' || data[2] != '2' || data[3] != '0') {
        return NULL;
    }
    for (t = 0; t < 5; t++) {
        if (data[4] == ok_tables[t][0] && data[5] == ok_tables[t][1] && data[6] == ok_tables[t][2] &&
            data[7] == ok_tables[t][3]) {
            break;
        }
    }
    if (t == 5) {
        return NULL;
    }
    raw = ((size_t)data[size - 4] << 16) | ((size_t)data[size - 3] << 8) | data[size - 2];
    skip = data[size - 1];
    if (raw == 0 || raw > max_out || skip > 31) {
        return NULL;
    }
    out = host->mem_alloc(host, raw, SEL_MEM_FAST);
    if (out == NULL) {
        return NULL;
    }
    b.d = data;
    b.pos = size - 4;
    b.lo = 8;
    b.cur = b.left = 0;
    b.bad = 0;
    for (i = 0; i < skip; i++) {
        pp_bit(&b);
    }
    pos = raw;
    while (pos > 0 && !b.bad) {
        unsigned idx, cnt, off, k;

        if (!pp_bit(&b)) {
            cnt = 1;
            do {
                k = pp_bits_n(&b, 2);
                cnt += k;
            } while (k == 3 && !b.bad);
            if (cnt > pos) {
                b.bad = 1;
                break;
            }
            while (cnt--) {
                out[--pos] = (uint8_t)pp_bits_n(&b, 8);
            }
            if (pos == 0) {
                break;
            }
        }
        idx = pp_bits_n(&b, 2);
        if (idx == 3) {
            off = pp_bits_n(&b, pp_bit(&b) ? data[7] : 7u) + 1;
            cnt = 5;
            do {
                k = pp_bits_n(&b, 3);
                cnt += k;
            } while (k == 7 && !b.bad);
        } else {
            cnt = idx + 2;
            off = pp_bits_n(&b, data[4 + idx]) + 1;
        }
        if (b.bad || cnt > pos || pos + off > raw) {
            b.bad = 1;
            break;
        }
        while (cnt--) {
            pos--;
            out[pos] = out[pos + off];
        }
    }
    if (b.bad) {
        host->mem_free(host, out);
        return NULL;
    }
    *out_size = raw;
    return out;
}
