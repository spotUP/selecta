/* HippoPlayer module program (.prg): "HiPPrg\n\n", then one entry per line:
 * key [#name=<text>] [#song=<digit>]. A line led by 0xF7 (UTF-8 c3 b7) is a
 * divider, one led by 0xA2 (UTF-8 c2 a2) is a stealth entry. Research:
 * thoughts/shared/research/2026-10-22_playlist-formats-and-hippoplayer-server.md */
#include "../playlist_internal.h"

#include <string.h>

#define DIV_MAGIC 0xF7u
#define STEALTH_MAGIC 0xA2u

static const char* find_marker(const char* s, size_t len, const char* marker) {
    size_t m = strlen(marker);
    size_t i;
    for (i = 0; i + m <= len; i++) {
        if (memcmp(s + i, marker, m) == 0) {
            return s + i;
        }
    }
    return NULL;
}

/* Strips a divider or stealth lead (Latin-1 byte or its UTF-8 form). */
static sel_pl_kind lead_kind(const char** line, size_t* n) {
    const unsigned char* b = (const unsigned char*)*line;
    size_t len = *n;
    if (len >= 1 && b[0] == DIV_MAGIC) {
        *line += 1;
        *n -= 1;
        return SEL_PL_DIVIDER;
    }
    if (len >= 2 && b[0] == 0xC3u && b[1] == 0xB7u) {
        *line += 2;
        *n -= 2;
        return SEL_PL_DIVIDER;
    }
    if (len >= 1 && b[0] == STEALTH_MAGIC) {
        *line += 1;
        *n -= 1;
        return SEL_PL_STEALTH;
    }
    if (len >= 2 && b[0] == 0xC2u && b[1] == 0xA2u) {
        *line += 2;
        *n -= 2;
        return SEL_PL_STEALTH;
    }
    return SEL_PL_SONG;
}

int sel_pl_read_hippo(const char* bytes, size_t len, sel_playlist* pl) {
    size_t pos = 0;
    const char* line;
    size_t n;
    int first = 1;
    while (sel_pl_next_line(bytes, len, &pos, &line, &n)) {
        const char* name = NULL;
        size_t name_len = 0;
        unsigned subsong = 0;
        const char* nm;
        const char* sg;
        size_t key_len;
        sel_pl_kind kind;
        int rc;
        if (first) {
            first = 0;
            if (n >= 6 && memcmp(line, "HiPPrg", 6) == 0) {
                continue;
            }
        }
        kind = lead_kind(&line, &n);
        if (kind == SEL_PL_SONG && n <= 5) {
            continue; /* HippoPlayer skips short lines other than dividers */
        }
        key_len = n;
        nm = find_marker(line, n, "#name=");
        sg = find_marker(line, n, "#song=");
        if (nm != NULL) {
            const char* end = line + n;
            const char* q;
            name = nm + 6;
            for (q = name; q < line + n; q++) {
                if (*q == '#') {
                    end = q;
                    break;
                }
            }
            name_len = (size_t)(end - name);
            key_len = (size_t)(nm - line);
        }
        if (sg != NULL) {
            if (sg + 6 < line + n && sg[6] >= '0' && sg[6] <= '9') {
                subsong = (unsigned)(sg[6] - '0');
            }
            if ((size_t)(sg - line) < key_len) {
                key_len = (size_t)(sg - line);
            }
        }
        if (kind == SEL_PL_DIVIDER) {
            name = line; /* a divider's text is its name */
            name_len = n;
            key_len = 0;
        }
        rc = sel_playlist_add(pl, kind, line, key_len, name, name_len, (uint8_t)subsong, 0);
        if (rc != SEL_PL_OK) {
            return rc;
        }
    }
    return SEL_PL_OK;
}

int sel_pl_write_hippo(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx) {
    uint32_t i;
    if (pl->count > SEL_PL_HIPPO_MAX) {
        return SEL_PL_RANGE;
    }
    sel_pl_puts(o, "HiPPrg\n\n");
    for (i = 0; i < pl->count; i++) {
        const sel_pl_entry* e = &pl->entries[i];
        if (e->kind == SEL_PL_DIVIDER) {
            sel_pl_put(o, "\xF7", 1);
            sel_pl_puts(o, sel_playlist_str(pl, e->name));
            sel_pl_put(o, "\n", 1);
            continue;
        }
        if (e->kind == SEL_PL_STEALTH) {
            sel_pl_put(o, "\xA2", 1);
        }
        sel_pl_puts(o, sel_playlist_str(pl, e->key));
        if (e->name != 0) {
            sel_pl_puts(o, "#name=");
            sel_pl_puts(o, sel_playlist_str(pl, e->name));
        }
        if (e->subsong > 9) {
            if (warn != NULL) {
                warn(warn_ctx, "lossy subsong"); /* the format holds one digit */
            }
        } else if (e->subsong > 0) {
            char digit = (char)('0' + e->subsong);
            sel_pl_puts(o, "#song=");
            sel_pl_put(o, &digit, 1);
        }
        sel_pl_put(o, "\n", 1);
    }
    return SEL_PL_OK;
}
