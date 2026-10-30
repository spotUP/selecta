/* The `hippo` source descriptor (plan Phase 4, I163; design doc 9.1 and 17): hippoplayer.se shared playlists.
 * A path is the server filename (`LMan_SID_Chip_Club.prg`); the song key is `hippo:<filename>` and the URL is
 * `https://hippoplayer.se/playlist/<filename>`. Entries are playlists, not tunes. */
#include "../source.h"
#include "hippo_net.h"
#include "hippo_sync.h"
#include "sync_host.h"

#include <stdio.h>
#include <string.h>

#define FORMAT_NAME "Hippo - Playlists"

static const sel_tag g_tags[] = {
    {SEL_SRC_HAS_LISTS, 1},
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    int w;

    if (path == NULL || path[0] == '\0' || strchr(path, '/') != NULL) {
        return 0;
    }
    w = snprintf(out, n, SEL_HIPPO_URL "%s", path);
    return w > 0 && (size_t)w < n;
}

static int path_in(const char* url, char* path, size_t n) {
    const char* rel;

    if (strncmp(url, SEL_HIPPO_URL, sizeof SEL_HIPPO_URL - 1u) != 0) {
        return 0;
    }
    rel = url + sizeof SEL_HIPPO_URL - 1u;
    if (rel[0] == '\0' || strchr(rel, '/') != NULL || strlen(rel) + 1u > n) {
        return 0;
    }
    memcpy(path, rel, strlen(rel) + 1u);
    return 1;
}

/* A filename only: format fixed, name = file without extension; the author comes from the list. */
static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* dot;
    size_t len, flen = sizeof FORMAT_NAME;

    if (path == NULL || path[0] == '\0' || strchr(path, '/') != NULL) {
        return 0;
    }
    dot = strrchr(path, '.');
    len = dot != NULL && dot != path ? (size_t)(dot - path) : strlen(path);
    if (flen + len + 1u > n) {
        return 0;
    }
    memcpy(scratch, FORMAT_NAME, flen);
    memcpy(scratch + flen, path, len);
    scratch[flen + len] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_NAME;
    out[1].value = (uintptr_t)(scratch + flen);
    out[2].tag = SEL_TAG_END;
    out[2].value = 0;
    return 1;
}

/* LIBRARY_SYNC dispatch (I517): a URL fetches both lists first (I718), anything else reads a directory of fetched
 * lists and .prg files. */
static int hippo_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    sel_hippo_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (strncmp(arg, "http://", 7) == 0 || strncmp(arg, "https://", 8) == 0) {
        sel_hippo_net_stats nt;

        ok = sel_hippo_net_sync(host, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_hippo_sync(sel_sync_cfg(host), arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.lists);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped);
    sel_sync_stat(stats, cap, SEL_SYNC_LISTS, st.lists);
    sel_sync_stat(stats, cap, SEL_SYNC_PRG_FILES, st.prg_files);
    sel_sync_stat(stats, cap, SEL_SYNC_PRG_ENTRIES, st.prg_entries);
    sel_sync_stat(stats, cap, SEL_SYNC_PRG_DIVIDERS, st.prg_dividers);
    sel_sync_stat(stats, cap, SEL_SYNC_PRG_STEALTH, st.prg_stealth);
    sel_sync_stat(stats, cap, SEL_SYNC_PRG_SUBSONGS, st.prg_subsongs);
    return ok;
}

static const sel_source g_hippo = {
    "hippo", "HippoPlayer", "https://hippoplayer.se/", "Shared HippoPlayer playlists (.prg).", g_tags,
    NULL, parse, NULL, url_of, path_in, NULL, NULL, hippo_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL, NULL,
};

const sel_source* sel_source_hippo(void) {
    return &g_hippo;
}
