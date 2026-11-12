/* nuade scan text (I1314c). Each row names where a player's own loader reads its title (and author) in the header,
 * so the scan row matches what the player reports at play time. Byte reads only: no allocation, no endianness. */
#include "scan_text.h"

#include <string.h>

typedef struct {
    const char* id;
    uint16_t title_at;
    uint8_t title_len;
    uint8_t author_len; /* author follows the title directly */
    const char* fixed_author; /* a player that reports the same artist for every file */
} scan_text_row;

static const scan_text_row k_rows[] = {
    { "soundmon", 0, 26, 0, NULL },        /* soundmon.c load_module: name at 0, mark at 26 */
    { "facethemusic", 16, 32, 32, NULL },  /* facethemusic.c: title at 16, artist at 48 */
    { "digmug", 80, 12, 0, NULL },         /* digmug.c: first sub-song name, 76 + 4 */
    { "instereo1", 36, 28, 0, NULL },      /* instereo1.c load_module: 8 + 14 header + 14 skipped */
    { "synthesis", 36, 28, 0, NULL },      /* synthesis.c: same layout, after the "Synth4.x" mark (4.2 at 0x1F0E) */
    { "quadracomposer", 22, 20, 0, NULL }, /* quadracomposer.c qc_parse_emic: EMIC body at 20, version word, name */
    { "laxity", 8, 16, 0, NULL },          /* laxity.c: name of record 0 (record size 0x18, name at +8) */
    { "professionalsoundartists", 4, 32, 0, NULL }, /* professionalsoundartists.c: PSA_TITLE_OFFSET, PSA_TITLE_BYTES */
    { "gluemon", 8, 8, 0, NULL },          /* gm_module.c read_header: name behind the 8 byte marks */
    { "protracker4", 8, 32, 0, NULL },     /* protracker4.c read_info: INFO chunk body at +8; chunk found by find_info below */
    { "synthpack", 0, 0, 0, "Karsten Obarski" },  /* synthpack_nuade.c: constant artist */
    { "wallybeben", 0, 0, 0, "Wally Beben" },     /* wallybeben_nuade.c: constant artist */
};

static void copy_text(const uint8_t* src, size_t len, char* out, size_t out_n) {
    size_t i, o = 0, keep = 0;
    for (i = 0; i < len && src[i] != 0u; i++) {
        unsigned c = src[i];
        size_t need = c < 0x80u ? 1u : 2u;
        if (c < 0x20u || o + need >= out_n) {
            break;
        }
        if (c < 0x80u) {
            out[o++] = (char)c;
        } else {
            out[o++] = (char)(0xC0u | (c >> 6));
            out[o++] = (char)(0x80u | (c & 0x3Fu));
        }
        if (c != 0x20u) {
            keep = o;
        }
    }
    out[keep] = '\0';
}

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* protracker4.c parse_module + find_chunk: FORM/MODL/VERS, then chunk to chunk by the size field until INFO.
 * Returns the offset of the INFO chunk, 0 when absent or short (INFO_BYTES 0x34). */
static size_t pt4_info_at(const uint8_t* data, size_t size) {
    uint64_t pos;
    uint32_t form_size;
    unsigned i;
    if (size < 0x20u || be32(data) != 0x464F524Du || be32(data + 8) != 0x4D4F444Cu || be32(data + 12) != 0x56455253u) {
        return 0;
    }
    form_size = be32(data + 4);
    if (form_size > size) {
        return 0;
    }
    pos = be32(data + 16) == 0x16u ? 0x1Eu : 12u;
    for (i = 0; i < 256u; i++) {
        uint32_t chunk_size;
        if (pos > size || size - pos < 8u) {
            return 0;
        }
        chunk_size = be32(data + pos + 4);
        if (chunk_size == 0u) {
            return 0;
        }
        if (be32(data + pos) == 0x494E464Fu) {
            return size - pos >= 0x34u ? (size_t)pos : 0u;
        }
        pos += chunk_size;
        if (pos >= form_size) {
            return 0;
        }
    }
    return 0;
}

int sel_nuade_scan_text(const char* id, const uint8_t* data, size_t size, char* title, size_t title_n, char* author,
                        size_t author_n) {
    size_t r, base = 0;
    if (title_n != 0u) {
        title[0] = '\0';
    }
    if (author_n != 0u) {
        author[0] = '\0';
    }
    for (r = 0; r < sizeof k_rows / sizeof k_rows[0]; r++) {
        const scan_text_row* row = &k_rows[r];
        size_t at;
        if (strcmp(row->id, id) != 0) {
            continue;
        }
        if (strcmp(id, "synthesis") == 0 && size >= 8u && memcmp(data, "Synth4.0", 8u) != 0) {
            base = 0x1F0Eu;
        }
        if (strcmp(id, "protracker4") == 0) {
            base = pt4_info_at(data, size);
            if (base == 0u) {
                return 1;
            }
        }
        at = base + row->title_at;
        if (title_n != 0u && size >= at + row->title_len) {
            copy_text(data + at, row->title_len, title, title_n);
        }
        at += row->title_len;
        if (author_n != 0u && row->author_len != 0u && size >= at + row->author_len) {
            copy_text(data + at, row->author_len, author, author_n);
        }
        if (author_n != 0u && row->fixed_author != NULL) {
            strncpy(author, row->fixed_author, author_n - 1u);
            author[author_n - 1u] = '\0';
        }
        return 1;
    }
    return 0;
}
