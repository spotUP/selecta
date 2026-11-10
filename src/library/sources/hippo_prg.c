/* See hippo_prg.h. */
#include "hippo_prg.h"

#include "../playlist/playlist.h"
#include "../song_key.h"

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

/* The song keys of a .prg, one per line, dividers and stealth entries left out (a divider is a heading, a stealth entry is
 * hidden by the list's author). Keys are written as the codec read them. Returns the byte count (0 when the codec refused or
 * nothing fits), `out` is always NUL terminated when cap > 0. */
size_t sel_hippo_prg_keys(const sel_host* host, const char* bytes, size_t len, char* out, size_t cap) {
    sel_playlist pl;
    sel_pl_entry* entries;
    char* text;
    char norm[SEL_SONG_KEY_MAX];
    uint32_t n = 1u, text_cap, k;
    size_t i, used = 0;

    if (cap == 0) {
        return 0;
    }
    out[0] = '\0';
    for (i = 0; i < len; i++) {
        n += bytes[i] == '\n' || bytes[i] == '\r';
    }
    if (n > SEL_PL_HIPPO_MAX) {
        return 0;
    }
    text_cap = (uint32_t)len * 2u + 8u;
    entries = host->mem_alloc(host, (size_t)n * sizeof *entries, SEL_MEM_FAST);
    text = host->mem_alloc(host, text_cap, SEL_MEM_FAST);
    if (entries != NULL && text != NULL) {
        sel_playlist_init(&pl, entries, n, text, text_cap);
        if (sel_playlist_read(SEL_PL_HIPPO, bytes, len, &pl, NULL, NULL) == SEL_PL_OK) {
            for (k = 0; k < pl.count; k++) {
                const char* key;
                size_t kl;

                if (entries[k].kind != SEL_PL_SONG) {
                    continue;
                }
                /* I1300: a raw URL of a known host is stored as its source key. */
                key = sel_playlist_str(&pl, entries[k].key);
                if (!sel_song_key_of_url(key, norm, sizeof norm)) {
                    continue;
                }
                key = norm;
                kl = strlen(key);
                if (used + kl + 2u > cap) {
                    break;
                }
                memcpy(out + used, key, kl);
                out[used + kl] = '\n';
                used += kl + 1u;
                out[used] = '\0';
            }
        }
    }
    if (entries != NULL) {
        host->mem_free(host, entries);
    }
    if (text != NULL) {
        host->mem_free(host, text);
    }
    return used;
}
