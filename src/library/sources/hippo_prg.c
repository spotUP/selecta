/* See hippo_prg.h. */
#include "hippo_prg.h"

#include "../playlist/playlist.h"

#include <string.h>

int sel_hippo_prg_scan(const sel_host* host, const char* bytes, size_t len, sel_hippo_prg_stats* out) {
    sel_playlist pl;
    sel_pl_entry* entries;
    char* text;
    uint32_t cap = 1u, text_cap;
    size_t i;
    uint32_t k;
    int rc;

    memset(out, 0, sizeof *out);
    for (i = 0; i < len; i++) {
        cap += bytes[i] == '\n' || bytes[i] == '\r';
    }
    if (cap > SEL_PL_HIPPO_MAX) {
        return 0;
    }
    text_cap = (uint32_t)len * 2u + 8u;
    entries = host->mem_alloc(host, (size_t)cap * sizeof *entries, SEL_MEM_FAST);
    text = host->mem_alloc(host, text_cap, SEL_MEM_FAST);
    if (entries == NULL || text == NULL) {
        rc = 0;
    } else {
        sel_playlist_init(&pl, entries, cap, text, text_cap);
        rc = sel_playlist_read(SEL_PL_HIPPO, bytes, len, &pl, NULL, NULL) == SEL_PL_OK;
        for (k = 0; rc && k < pl.count; k++) {
            out->entries++;
            out->dividers += entries[k].kind == SEL_PL_DIVIDER;
            out->stealth += entries[k].kind == SEL_PL_STEALTH;
            out->subsongs += entries[k].subsong != 0;
        }
    }
    if (entries != NULL) {
        host->mem_free(host, entries);
    }
    if (text != NULL) {
        host->mem_free(host, text);
    }
    return rc;
}
