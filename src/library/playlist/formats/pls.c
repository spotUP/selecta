/* PLS (INI): [playlist], File<n>=, Title<n>=, Length<n>= (seconds),
 * NumberOfEntries=, Version=2. No divider, subsong or stealth. File<n> must
 * come before its Title<n> and Length<n>. */
#include "../playlist_internal.h"

#include <string.h>

static int ieq_prefix(const char* s, size_t n, const char* prefix) {
    size_t i;
    size_t m = strlen(prefix);
    if (n < m) {
        return 0;
    }
    for (i = 0; i < m; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != prefix[i]) {
            return 0;
        }
    }
    return 1;
}

int sel_pl_read_pls(const char* bytes, size_t len, sel_playlist* pl) {
    size_t pos = 0;
    const char* line;
    size_t n;
    uint32_t base = pl->count; /* entries of this file start here */
    while (sel_pl_next_line(bytes, len, &pos, &line, &n)) {
        int is_file = ieq_prefix(line, n, "file");
        int is_title = ieq_prefix(line, n, "title");
        int is_len = ieq_prefix(line, n, "length");
        size_t used = 0;
        size_t skip = is_file ? 4u : (is_title ? 5u : 6u);
        uint32_t idx;
        const char* val;
        size_t vlen;
        if (!is_file && !is_title && !is_len) {
            continue;
        }
        idx = sel_pl_parse_u(line + skip, n - skip, &used);
        if (used == 0 || skip + used >= n || line[skip + used] != '=' || idx == 0) {
            continue;
        }
        val = line + skip + used + 1;
        vlen = n - skip - used - 1;
        if (is_file) {
            int rc = sel_playlist_add(pl, SEL_PL_SONG, val, vlen, NULL, 0, 0, 0);
            if (rc != SEL_PL_OK) {
                return rc;
            }
        } else if (base + idx <= pl->count) {
            sel_pl_entry* e = &pl->entries[base + idx - 1u];
            if (is_title) {
                int rc = sel_playlist_set_name(pl, base + idx - 1u, val, vlen);
                if (rc != SEL_PL_OK) {
                    return rc;
                }
            } else if (vlen > 0 && val[0] != '-') {
                size_t u2 = 0;
                e->duration_ms = sel_pl_parse_u(val, vlen, &u2) * 1000u;
            }
        }
    }
    return SEL_PL_OK;
}

int sel_pl_write_pls(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx) {
    uint32_t i;
    uint32_t n = 0;
    sel_pl_puts(o, "[playlist]\n");
    for (i = 0; i < pl->count; i++) {
        const sel_pl_entry* e = &pl->entries[i];
        if (e->kind == SEL_PL_DIVIDER) {
            if (warn != NULL) {
                warn(warn_ctx, "lossy divider");
            }
            continue;
        }
        n++;
        sel_pl_puts(o, "File");
        sel_pl_putu(o, n);
        sel_pl_puts(o, "=");
        sel_pl_puts(o, sel_playlist_str(pl, e->key));
        sel_pl_puts(o, "\n");
        if (e->name != 0) {
            sel_pl_puts(o, "Title");
            sel_pl_putu(o, n);
            sel_pl_puts(o, "=");
            sel_pl_puts(o, sel_playlist_str(pl, e->name));
            sel_pl_puts(o, "\n");
        }
        if (e->duration_ms != 0) {
            sel_pl_puts(o, "Length");
            sel_pl_putu(o, n);
            sel_pl_puts(o, "=");
            sel_pl_putu(o, e->duration_ms / 1000u);
            sel_pl_puts(o, "\n");
        }
    }
    sel_pl_puts(o, "NumberOfEntries=");
    sel_pl_putu(o, n);
    sel_pl_puts(o, "\nVersion=2\n");
    return SEL_PL_OK;
}
