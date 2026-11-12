/* nuade scan text (I1314c). Each row names where a player's own loader reads its title (and author) in the header,
 * so the scan row matches what the player reports at play time. Byte reads only: no allocation, no endianness. */
#include "scan_text.h"

#include <string.h>

typedef struct {
    const char* id;
    uint16_t title_at;
    uint8_t title_len;
    uint8_t author_len; /* author follows the title directly */
} scan_text_row;

static const scan_text_row k_rows[] = {
    { "soundmon", 0, 26, 0 },        /* soundmon.c load_module: name at 0, mark at 26 */
    { "facethemusic", 16, 32, 32 },  /* facethemusic.c: title at 16, artist at 48 */
    { "digmug", 80, 12, 0 },         /* digmug.c: first sub-song name, 76 + 4 */
    { "instereo1", 36, 28, 0 },      /* instereo1.c load_module: 8 + 14 header + 14 skipped */
    { "synthesis", 36, 28, 0 },      /* synthesis.c: same layout, after the "Synth4.x" mark (4.2 at 0x1F0E) */
    { "quadracomposer", 22, 20, 0 }, /* quadracomposer.c qc_parse_emic: EMIC body at 20, version word, name */
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
        at = base + row->title_at;
        if (title_n != 0u && size >= at + row->title_len) {
            copy_text(data + at, row->title_len, title, title_n);
        }
        at += row->title_len;
        if (author_n != 0u && row->author_len != 0u && size >= at + row->author_len) {
            copy_text(data + at, row->author_len, author, author_n);
        }
        return 1;
    }
    return 0;
}
