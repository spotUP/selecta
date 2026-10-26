/* The `asma` source descriptor (row I413; online-sources plan section 2; design doc 9.1): ASMA, the Atari SAP
 * Music Archive. A path is relative to the /asma/ root: `Composers/Aki/Atari_Style.sap`, `Games/221B_Baker_Street.sap`;
 * the song key is `asma:<path>` and the canonical URL is https://asma.atari.org/asma/<path>, percent-encoded. */
#include "../source.h"
#include "asma_net.h"
#include "modland_net.h"
#include "sync_host.h"
#include "asma_sync.h"
#include "source/url_path.h"

#include <stdio.h>
#include <string.h>

#define ROOT "https://asma.atari.org/" SEL_ASMA_ROOT

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    return sel_url_path_encode(ROOT, path, out, n);
}

static int path_in(const char* url, char* path, size_t n) {
    return sel_url_path_decode(ROOT, url, path, n);
}

/* Format from the extension; the artist of `Composers/<Folder>/...` is the folder ("Bernard_Jeremie" ->
 * "Bernard Jeremie"; the sync overrides it with the author of the list); name = file without extension. */
static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* file;
    const char* dot;
    const char* artist = "";
    size_t artist_len = 0, fmt_len, name_len, i;
    const char* fmt;

    if (path == NULL || (file = strrchr(path, '/')) == NULL || file[1] == '\0') {
        file = path != NULL ? path : "";
        if (file[0] == '\0') {
            return 0;
        }
    } else {
        file++;
    }
    dot = strrchr(file, '.');
    fmt = dot != NULL && strcmp(dot, ".ttt") == 0 ? "TIATracker" : "Slight Atari Player";
    if (strncmp(path, "Composers/", 10) == 0) {
        const char* e = strchr(path + 10, '/');

        if (e != NULL) {
            artist = path + 10;
            artist_len = (size_t)(e - artist);
        }
    }
    fmt_len = strlen(fmt);
    name_len = dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file);
    if (fmt_len + artist_len + name_len + 3u > n) {
        return 0;
    }
    memcpy(scratch, fmt, fmt_len + 1u);
    for (i = 0; i < artist_len; i++) {
        scratch[fmt_len + 1u + i] = artist[i] == '_' ? ' ' : artist[i];
    }
    scratch[fmt_len + 1u + artist_len] = '\0';
    memcpy(scratch + fmt_len + artist_len + 2u, file, name_len);
    scratch[fmt_len + artist_len + 2u + name_len] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + fmt_len + 1u);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + fmt_len + artist_len + 2u);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

/* LIBRARY_SYNC dispatch (I517): a URL runs the network sync, anything else reads a local mirror copy. */
static int asma_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_local_scan_config* cfg = sel_sync_cfg(host);
    sel_asma_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (sel_modland_is_url(arg)) {
        sel_asma_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_asma_net_sync(cfg, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, nt.list_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_asma_sync(cfg, arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    return ok;
}

static const sel_source g_asma = {
    "asma", "ASMA", "https://asma.atari.org/", "Atari SAP Music Archive: music of the Atari 8-bit computers.", g_tags,
    NULL, parse, NULL, url_of, path_in, NULL, NULL, asma_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL,
};

const sel_source* sel_source_asma(void) {
    return &g_asma;
}
