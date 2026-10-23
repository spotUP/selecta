/* M3U / M3U8: "#EXTM3U", then optional "#EXTINF:<seconds>,<title>" before the
 * key line. Other # lines are comments. M3U has no divider, no subsong and no
 * stealth: a divider is dropped with the warning "lossy divider". */
#include "../playlist_internal.h"

#include <string.h>

int sel_pl_read_m3u(const char* bytes, size_t len, sel_playlist* pl) {
    size_t pos = 0;
    const char* line;
    size_t n;
    const char* title = NULL;
    size_t title_len = 0;
    uint32_t duration_ms = 0;
    while (sel_pl_next_line(bytes, len, &pos, &line, &n)) {
        int rc;
        if (n == 0) {
            continue;
        }
        if (line[0] == '#') {
            if (n > 8 && memcmp(line, "#EXTINF:", 8) == 0) {
                size_t used = 0;
                size_t i;
                const char* p = line + 8;
                size_t rest = n - 8;
                int neg = rest > 0 && p[0] == '-';
                uint32_t sec = sel_pl_parse_u(p + (neg ? 1 : 0), rest - (neg ? 1u : 0u), &used);
                duration_ms = neg ? 0 : sec * 1000u;
                title = NULL;
                title_len = 0;
                for (i = 0; i < rest; i++) {
                    if (p[i] == ',') {
                        title = p + i + 1;
                        title_len = rest - i - 1;
                        break;
                    }
                }
            }
            continue;
        }
        rc = sel_playlist_add(pl, SEL_PL_SONG, line, n, title, title_len, 0, duration_ms);
        if (rc != SEL_PL_OK) {
            return rc;
        }
        title = NULL;
        title_len = 0;
        duration_ms = 0;
    }
    return SEL_PL_OK;
}

int sel_pl_write_m3u(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn warn, void* warn_ctx) {
    uint32_t i;
    sel_pl_puts(o, "#EXTM3U\n");
    for (i = 0; i < pl->count; i++) {
        const sel_pl_entry* e = &pl->entries[i];
        if (e->kind == SEL_PL_DIVIDER) {
            if (warn != NULL) {
                warn(warn_ctx, "lossy divider");
            }
            continue;
        }
        if (e->name != 0 || e->duration_ms != 0) {
            sel_pl_puts(o, "#EXTINF:");
            if (e->duration_ms == 0) {
                sel_pl_puts(o, "-1");
            } else {
                sel_pl_putu(o, e->duration_ms / 1000u);
            }
            sel_pl_puts(o, ",");
            sel_pl_puts(o, sel_playlist_str(pl, e->name));
            sel_pl_puts(o, "\n");
        }
        sel_pl_puts(o, sel_playlist_str(pl, e->key));
        sel_pl_puts(o, "\n");
    }
    return SEL_PL_OK;
}
