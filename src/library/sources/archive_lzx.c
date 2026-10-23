/* Amiga LZX archive reader, written for Selecta (I159a). Format knowledge from the public unlzx
 * description and Teemu Suutari's `ancient` (BSD 2-Clause) as reference; no code is copied.
 *
 * Archive: 10 byte header "LZX...", then entries: a 31 byte header (+2 unpacked size, +6 packed size,
 * +11 pack mode 0 stored / 2 LZX, +14 comment length, +22 data CRC32, +26 header CRC32, +30 name
 * length; all little endian), the name, the comment and the packed data. Consecutive entries with
 * packed size 0 and unpacked size > 0 form a merged group: the next entry with packed data carries
 * one LZX stream for the whole group.
 *
 * LZX stream: 16 bit big endian words consumed least significant bit first; Huffman codes are
 * canonical (shortest first, then symbol order) and arrive most significant bit first. */
#include "archive.h"

#include <miniz.h>
#include <string.h>

#define LZX_HEAD 31u
#define LZX_GROUP_MAX 256u
#define LZX_TOTAL_MAX (32u * 1024u * 1024u)
#define LZX_NAME_MAX 512u

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- bit reader ---- */

typedef struct lz_bits {
    const uint8_t* d;
    size_t size, pos;
    uint32_t buf;
    unsigned have;
    int bad;
} lz_bits;

static unsigned lz_read(lz_bits* b, unsigned n) {
    unsigned v;

    if (n == 0) {
        return 0;
    }
    while (b->have < n) {
        uint32_t w = 0;

        if (b->pos + 1 < b->size) {
            w = ((uint32_t)b->d[b->pos] << 8) | b->d[b->pos + 1];
        } else if (b->pos >= b->size + 4) {
            b->bad = 1; /* far past the end: damaged */
        }
        b->pos += 2;
        b->buf |= w << b->have;
        b->have += 16;
    }
    v = b->buf & ((1u << n) - 1u);
    b->buf >>= n;
    b->have -= n;
    return v;
}

/* ---- canonical Huffman ---- */

#define HUFF_MAXBITS 16u

typedef struct lz_huff {
    uint16_t count[HUFF_MAXBITS + 1];
    uint16_t sym[768];
    unsigned n;
} lz_huff;

static void huff_build(lz_huff* h, const uint8_t* len, unsigned n) {
    uint16_t offs[HUFF_MAXBITS + 2];
    unsigned i;

    memset(h->count, 0, sizeof h->count);
    h->n = n;
    for (i = 0; i < n; i++) {
        h->count[len[i]]++;
    }
    h->count[0] = 0;
    offs[1] = 0;
    for (i = 1; i <= HUFF_MAXBITS; i++) {
        offs[i + 1] = (uint16_t)(offs[i] + h->count[i]);
    }
    for (i = 0; i < n; i++) {
        if (len[i]) {
            h->sym[offs[len[i]]++] = (uint16_t)i;
        }
    }
}

/* Returns the symbol, or -1 on an invalid code. */
static int huff_decode(lz_bits* b, const lz_huff* h) {
    int code = 0, first = 0, index = 0;
    unsigned len;

    for (len = 1; len <= HUFF_MAXBITS; len++) {
        int cnt;

        code |= (int)lz_read(b, 1);
        cnt = h->count[len];
        if (code - cnt < first) {
            return h->sym[index + (code - first)];
        }
        index += cnt;
        first += cnt;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* ---- LZX stream ---- */

static const uint16_t kv_base[32] = {0,   1,   2,   3,    4,    6,    8,    12,   16,   24,   32,
                                     48,  64,  96,  128,  192,  256,  384,  512,  768,  1024, 1536,
                                     2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152};
static const uint8_t kv_extra[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3,  3,  4,  4,  5,  5,  6,  6,
                                     7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

/* Reads one 768 entry literal length table in two passes. Returns 0 when damaged. */
static int read_literal_table(lz_bits* b, uint8_t* lens) {
    unsigned pass;

    for (pass = 0; pass < 2; pass++) {
        unsigned first = pass ? 256u : 0u, last = pass ? 768u : 256u, adj = pass ? 0u : 1u, i, pos;
        uint8_t pre[20];
        lz_huff pt;

        for (i = 0; i < 20; i++) {
            pre[i] = (uint8_t)lz_read(b, 4);
        }
        huff_build(&pt, pre, 20);
        pos = first;
        while (pos < last && !b->bad) {
            int s = huff_decode(b, &pt);
            unsigned n, v;

            if (s < 0) {
                return 0;
            }
            if (s == 17 || s == 18) {
                n = s == 17 ? lz_read(b, 4) + 3 + adj : lz_read(b, 6 - adj) + 19 + adj;
                if (n > last - pos) {
                    return 0;
                }
                while (n--) {
                    lens[pos++] = 0;
                }
            } else if (s == 19) {
                n = lz_read(b, 1) + 3 + adj;
                s = huff_decode(b, &pt);
                if (s < 0 || n > last - pos) {
                    return 0;
                }
                v = (lens[pos] + 17u - (unsigned)s) % 17u;
                while (n--) {
                    lens[pos++] = (uint8_t)v;
                }
            } else {
                lens[pos] = (uint8_t)((lens[pos] + 17u - (unsigned)s) % 17u);
                pos++;
            }
        }
    }
    return !b->bad;
}

/* Decodes `total` bytes of one stream into out. Returns 0 when damaged. */
static int lzx_decode(const uint8_t* in, size_t in_size, uint8_t* out, size_t total) {
    lz_bits b;
    uint8_t lens[768];
    uint8_t alens[8];
    lz_huff lit, aln;
    size_t outp = 0;
    unsigned last_dist = 0;
    int have_lit = 0;

    b.d = in;
    b.size = in_size;
    b.pos = 0;
    b.buf = 0;
    b.have = 0;
    b.bad = 0;
    memset(lens, 0, sizeof lens);
    while (outp < total && !b.bad) {
        unsigned method = lz_read(&b, 3), i;
        size_t block;

        if (method < 1 || method > 3) {
            return 0;
        }
        if (method == 3) {
            for (i = 0; i < 8; i++) {
                alens[i] = (uint8_t)lz_read(&b, 3);
            }
            huff_build(&aln, alens, 8);
        }
        block = (size_t)lz_read(&b, 8) << 16;
        block |= (size_t)lz_read(&b, 8) << 8;
        block |= lz_read(&b, 8);
        if (method != 1) {
            if (!read_literal_table(&b, lens)) {
                return 0;
            }
            huff_build(&lit, lens, 768);
            have_lit = 1;
        } else if (!have_lit) {
            return 0;
        }
        if (block > total - outp) {
            block = total - outp;
        }
        while (block > 0 && !b.bad) {
            int s = huff_decode(&b, &lit);
            unsigned slot, cnt, extra, dist;

            if (s < 0) {
                return 0;
            }
            if (s < 256) {
                out[outp++] = (uint8_t)s;
                block--;
                continue;
            }
            s -= 256;
            slot = (unsigned)s & 31u;
            cnt = ((unsigned)s >> 5) + 3u;
            extra = kv_extra[slot];
            dist = kv_base[slot];
            if (method == 3 && extra >= 3) {
                int a;

                dist += lz_read(&b, extra - 3) << 3;
                a = huff_decode(&b, &aln);
                if (a < 0) {
                    return 0;
                }
                dist += (unsigned)a;
            } else {
                dist += lz_read(&b, extra);
            }
            if (dist == 0) {
                dist = last_dist;
            }
            last_dist = dist;
            if (dist == 0 || dist > outp || cnt > block) {
                return 0;
            }
            for (i = 0; i < cnt; i++) {
                out[outp] = out[outp - dist];
                outp++;
            }
            block -= cnt;
        }
    }
    return !b.bad && outp == total;
}

/* ---- archive walk ---- */

typedef struct lz_entry {
    const uint8_t* name;
    unsigned name_len;
    uint32_t size, crc;
} lz_entry;

static int header_ok(const uint8_t* h, unsigned name_len, unsigned comment_len) {
    uint8_t copy[LZX_HEAD];
    uint32_t crc;

    memcpy(copy, h, LZX_HEAD);
    memset(copy + 26, 0, 4);
    crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, copy, LZX_HEAD);
    crc = (uint32_t)mz_crc32(crc, h + LZX_HEAD, name_len);
    crc = (uint32_t)mz_crc32(crc, h + LZX_HEAD + name_len, comment_len);
    return crc == le32(h + 26);
}

static int deliver(const lz_entry* e, const uint8_t* bytes, size_t max_member, int list_only,
                   sel_archive_member_fn fn, void* ctx, int* stop) {
    char name[LZX_NAME_MAX];
    unsigned n = e->name_len < LZX_NAME_MAX - 1 ? e->name_len : LZX_NAME_MAX - 1;

    if (e->size == 0 || e->size > max_member) {
        return 0;
    }
    if (!list_only && mz_crc32(MZ_CRC32_INIT, bytes, e->size) != e->crc) {
        return 0; /* member damaged: skipped */
    }
    memcpy(name, e->name, n);
    name[n] = 0;
    if (!fn(ctx, name, list_only ? NULL : bytes, e->size)) {
        *stop = 1;
    }
    return 1;
}

int sel_lzx_walk(const sel_host* host, const uint8_t* data, size_t size, size_t max_member, int list_only,
                 sel_archive_member_fn fn, void* ctx) {
    lz_entry group[LZX_GROUP_MAX];
    unsigned group_n = 0;
    size_t pos = 10;
    int delivered = 0, stop = 0;

    if (size < 10 || data[0] != 'L' || data[1] != 'Z' || data[2] != 'X') {
        return -1;
    }
    while (pos + LZX_HEAD <= size && !stop) {
        const uint8_t* h = data + pos;
        uint32_t unp = le32(h + 2), pk = le32(h + 6);
        unsigned mode = h[11], comment = h[14], nlen = h[30];
        size_t data_at = pos + LZX_HEAD + nlen + comment;

        if (data_at > size || pk > size - data_at || !header_ok(h, nlen, comment)) {
            return -1;
        }
        pos = data_at + pk;
        if (unp == 0 && pk == 0) {
            continue; /* directory or empty file */
        }
        if (group_n >= LZX_GROUP_MAX) {
            return -1;
        }
        group[group_n].name = h + LZX_HEAD;
        group[group_n].name_len = nlen;
        group[group_n].size = unp;
        group[group_n].crc = le32(h + 22);
        group_n++;
        if (pk == 0) {
            continue; /* merged: the stream comes with a later entry */
        }
        if (list_only) {
            unsigned i;

            for (i = 0; i < group_n && !stop; i++) {
                delivered += deliver(&group[i], NULL, max_member, 1, fn, ctx, &stop);
            }
        } else if (mode == 0) {
            if (group_n == 1 && unp <= pk) {
                delivered += deliver(&group[0], data + data_at, max_member, 0, fn, ctx, &stop);
            }
        } else if (mode == 2) {
            uint64_t total = 0;
            unsigned i;
            uint8_t* out;

            for (i = 0; i < group_n; i++) {
                total += group[i].size;
            }
            if (total <= LZX_TOTAL_MAX && (out = host->mem_alloc(host, (size_t)total, SEL_MEM_FAST)) != NULL) {
                if (lzx_decode(data + data_at, pk, out, (size_t)total)) {
                    size_t at = 0;

                    for (i = 0; i < group_n && !stop; i++) {
                        delivered += deliver(&group[i], out + at, max_member, 0, fn, ctx, &stop);
                        at += group[i].size;
                    }
                }
                host->mem_free(host, out);
            }
        }
        group_n = 0;
    }
    return delivered;
}
