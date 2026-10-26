#ifndef SELECTA_STREAM_FLAC_META_H
#define SELECTA_STREAM_FLAC_META_H
/* FLAC metadata reader (I437): walks the metadata blocks of a file and returns the PICTURE block (front cover
 * type 3 preferred, else the first) plus the ALBUM and ARTIST Vorbis comments (the album key of the artwork row).
 * Pure functions over the byte buffer, no allocation. A block that runs past the buffer is rejected. */
#include "id3.h"

static inline size_t sel_flac_le32(const uint8_t* p) {
    return (size_t)p[0] | ((size_t)p[1] << 8) | ((size_t)p[2] << 16) | ((size_t)p[3] << 24);
}

/* Copies value of `KEY=value` (case-insensitive key) into dst when the comment matches. */
static inline void sel_flac_comment(const uint8_t* s, size_t n, const char* key, char* dst, size_t cap) {
    size_t k = strlen(key), i;
    if (dst[0] != '\0' || n <= k || s[k] != '=') {
        return;
    }
    for (i = 0; i < k; i++) {
        uint8_t ch = s[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (uint8_t)(ch - 32u);
        }
        if (ch != (uint8_t)key[i]) {
            return;
        }
    }
    n -= k + 1u;
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(dst, s + k + 1u, n);
    dst[n] = '\0';
}

static inline void sel_flac_vorbis(const uint8_t* b, size_t n, char* album, char* artist, size_t cap) {
    size_t i, count, v;
    if (n < 8u) {
        return;
    }
    v = sel_flac_le32(b);
    if (v > n - 8u) {
        return;
    }
    i = 4u + v;
    count = sel_flac_le32(b + i);
    i += 4u;
    for (; count > 0u && i + 4u <= n; count--) {
        size_t l = sel_flac_le32(b + i);
        i += 4u;
        if (l > n - i) {
            return;
        }
        sel_flac_comment(b + i, l, "ALBUM", album, cap);
        sel_flac_comment(b + i, l, "ARTIST", artist, cap);
        i += l;
    }
}

static inline void sel_flac_picture(const uint8_t* b, size_t n, size_t max, sel_cover* c) {
    size_t i = 0, ml, dl, pl;
    int type;
    if (n < 32u) {
        return;
    }
    type = (int)sel_id3_be32(b);
    ml = sel_id3_be32(b + 4);
    i = 8u;
    if (ml > n - i || n - i - ml < 4u) {
        return;
    }
    {
        const char* mime = (const char*)b + i;
        i += ml;
        dl = sel_id3_be32(b + i);
        i += 4u;
        if (dl > n - i || n - i - dl < 20u) {
            return;
        }
        i += dl + 16u; /* description, width, height, depth, colours */
        pl = sel_id3_be32(b + i);
        i += 4u;
        if (pl == 0u || pl > n - i || pl > max) {
            return;
        }
        sel_cover_offer(c, b + i, pl, mime, ml, type);
    }
}

/* Returns 1 when a picture was found. album/artist (cap bytes each) are filled when the file has the comments. */
static inline int sel_flac_meta(const uint8_t* d, size_t size, size_t max, sel_cover* c, char* album, char* artist,
                                size_t cap) {
    size_t pos = 4u;
    int last = 0;
    memset(c, 0, sizeof *c);
    album[0] = artist[0] = '\0';
    if (size < 8u || memcmp(d, "fLaC", 4) != 0) {
        return 0;
    }
    while (!last && pos + 4u <= size) {
        unsigned type = d[pos] & 0x7Fu;
        size_t len = ((size_t)d[pos + 1u] << 16) | ((size_t)d[pos + 2u] << 8) | d[pos + 3u];
        last = (d[pos] & 0x80u) != 0u;
        pos += 4u;
        if (len > size - pos) {
            break; /* truncated block */
        }
        if (type == 6u) {
            sel_flac_picture(d + pos, len, max, c);
        } else if (type == 4u) {
            sel_flac_vorbis(d + pos, len, album, artist, cap);
        }
        pos += len;
    }
    return c->data != NULL;
}

#endif /* SELECTA_STREAM_FLAC_META_H */
