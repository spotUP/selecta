#ifndef SELECTA_STREAM_ID3_H
#define SELECTA_STREAM_ID3_H
/* ID3 tag reader for the MP3 replayer (I194b): ID3v2.3 and v2.4 text frames TIT2, TPE1, TALB in text
 * encodings 0 (ISO-8859-1), 1 (UTF-16 with BOM), 2 (UTF-16BE), 3 (UTF-8), converted to UTF-8; the ID3v1
 * block at the end of the file fills any field the v2 tag left empty. Pure functions over the byte buffer,
 * no allocation. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct sel_id3 {
    char title[128], artist[128], album[128];
} sel_id3;

/* Appends code point `cp` as UTF-8 at dst[*o], keeping one byte for the terminator. */
static inline void sel_id3_put(char* dst, size_t cap, size_t* o, uint32_t cp) {
    uint8_t b[4];
    size_t n, i;
    if (cp < 0x80u) {
        b[0] = (uint8_t)cp;
        n = 1;
    } else if (cp < 0x800u) {
        b[0] = (uint8_t)(0xC0u | (cp >> 6));
        b[1] = (uint8_t)(0x80u | (cp & 0x3Fu));
        n = 2;
    } else if (cp < 0x10000u) {
        b[0] = (uint8_t)(0xE0u | (cp >> 12));
        b[1] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (uint8_t)(0x80u | (cp & 0x3Fu));
        n = 3;
    } else {
        b[0] = (uint8_t)(0xF0u | (cp >> 18));
        b[1] = (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (uint8_t)(0x80u | (cp & 0x3Fu));
        n = 4;
    }
    if (*o + n >= cap) {
        return;
    }
    for (i = 0; i < n; i++) {
        dst[(*o)++] = (char)b[i];
    }
}

static inline uint32_t sel_id3_u16(const uint8_t* p, int be) {
    return be ? (uint32_t)((uint32_t)p[0] << 8 | p[1]) : (uint32_t)((uint32_t)p[1] << 8 | p[0]);
}

/* Text of one frame body (encoding byte first) into dst as UTF-8, up to the first terminator. */
static inline void sel_id3_text(const uint8_t* p, size_t n, char* dst, size_t cap) {
    size_t o = 0, i = 1;
    unsigned enc;
    dst[0] = '\0';
    if (n < 1u) {
        return;
    }
    enc = p[0];
    if (enc == 0u) {
        for (; i < n && p[i] != 0u; i++) {
            sel_id3_put(dst, cap, &o, p[i]);
        }
    } else if (enc == 3u) {
        for (; i < n && p[i] != 0u && o + 1u < cap; i++) {
            dst[o++] = (char)p[i];
        }
    } else if (enc == 1u || enc == 2u) {
        int be = enc == 2u;
        if (enc == 1u && n >= 3u) {
            if (p[1] == 0xFEu && p[2] == 0xFFu) {
                be = 1;
                i = 3;
            } else if (p[1] == 0xFFu && p[2] == 0xFEu) {
                i = 3;
            }
        }
        while (i + 1u < n) {
            uint32_t u = sel_id3_u16(p + i, be);
            i += 2u;
            if (u == 0u) {
                break;
            }
            if (u >= 0xD800u && u < 0xDC00u && i + 1u < n) {
                uint32_t lo = sel_id3_u16(p + i, be);
                if (lo >= 0xDC00u && lo < 0xE000u) {
                    u = 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
                    i += 2u;
                }
            }
            sel_id3_put(dst, cap, &o, u);
        }
    }
    dst[o] = '\0';
}

static inline size_t sel_id3_syncsafe(const uint8_t* p) {
    return ((size_t)p[0] << 21) | ((size_t)p[1] << 14) | ((size_t)p[2] << 7) | p[3];
}

static inline size_t sel_id3_be32(const uint8_t* p) {
    return ((size_t)p[0] << 24) | ((size_t)p[1] << 16) | ((size_t)p[2] << 8) | p[3];
}

/* One ID3v1 field: 30 bytes of ISO-8859-1, space or NUL padded. */
static inline void sel_id3v1_field(const uint8_t* p, char* dst, size_t cap) {
    size_t n = 30, o = 0, i;
    while (n > 0u && (p[n - 1u] == 0u || p[n - 1u] == ' ')) {
        n--;
    }
    for (i = 0; i < n && p[i] != 0u; i++) {
        sel_id3_put(dst, cap, &o, p[i]);
    }
    dst[o] = '\0';
}

static inline void sel_id3_read(const uint8_t* d, size_t size, sel_id3* t) {
    memset(t, 0, sizeof *t);
    if (size >= 10u && memcmp(d, "ID3", 3) == 0 && (d[3] == 3u || d[3] == 4u) &&
        ((d[6] | d[7] | d[8] | d[9]) & 0x80u) == 0u) {
        unsigned ver = d[3];
        size_t end = 10u + sel_id3_syncsafe(d + 6), pos = 10u;
        if (end > size) {
            end = size;
        }
        if ((d[5] & 0x40u) != 0u && pos + 4u <= end) { /* extended header */
            pos += ver == 4u ? sel_id3_syncsafe(d + pos) : 4u + sel_id3_be32(d + pos);
        }
        while (pos + 10u <= end && d[pos] != 0u) {
            const uint8_t* f = d + pos;
            size_t len = ver == 4u ? sel_id3_syncsafe(f + 4) : sel_id3_be32(f + 4);
            unsigned skip = ver == 4u ? (f[9] & 0x0Cu) : (f[9] & 0xC0u); /* compressed or encrypted */
            const uint8_t* body = f + 10;
            size_t used;
            char* dst = NULL;
            if (len > end - pos - 10u) {
                break;
            }
            used = len;
            if (ver == 4u && (f[9] & 0x01u) != 0u && len >= 4u) { /* data length indicator */
                body += 4;
                len -= 4u;
            }
            if (memcmp(f, "TIT2", 4) == 0) {
                dst = t->title;
            } else if (memcmp(f, "TPE1", 4) == 0) {
                dst = t->artist;
            } else if (memcmp(f, "TALB", 4) == 0) {
                dst = t->album;
            }
            if (dst != NULL && skip == 0u) {
                sel_id3_text(body, len, dst, 128u);
            }
            pos += 10u + used;
        }
    }
    if (size >= 128u && memcmp(d + size - 128u, "TAG", 3) == 0) {
        const uint8_t* v1 = d + size - 128u;
        if (t->title[0] == '\0') {
            sel_id3v1_field(v1 + 3, t->title, sizeof t->title);
        }
        if (t->artist[0] == '\0') {
            sel_id3v1_field(v1 + 33, t->artist, sizeof t->artist);
        }
        if (t->album[0] == '\0') {
            sel_id3v1_field(v1 + 63, t->album, sizeof t->album);
        }
    }
}

#endif /* SELECTA_STREAM_ID3_H */
