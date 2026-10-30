/* The `hvsc` source descriptor (plan Phase 4, I162; design doc 9.1): the High Voltage SID Collection,
 * hosted by Modland. A path is relative to the HVSC root (`C64Music/`): `MUSICIANS/H/Hubbard_Rob/Commando.sid`,
 * `GAMES/A/Aztec_Challenge.sid`, `DEMOS/0-9/1_45_Demo.sid`; the song key is `hvsc:<path>`. Files are served
 * through Modland's path layout: the host path is `pub/modules/HVSC/<path>` (SEL_SRC_HOSTED_BY = modland),
 * and the canonical URL is the Modland URL of the host path. Format names follow bbmp: "HVSC - Musicians",
 * "HVSC - Games", "HVSC - Demos". */
#include "hvsc_net.h"
#include "hvsc_sync.h"

#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"
#include "source/url_path.h"

#include <stdio.h>
#include <string.h>

#define ROOT "https://modland.com/"

static const sel_tag g_tags[] = {
    {SEL_SRC_HOSTED_BY, (uintptr_t)"modland"},
    {SEL_TAG_END, 0},
};

int sel_hvsc_host_path(const char* path, char* out, size_t n) {
    int w;

    if (path == NULL || path[0] == '\0' || path[0] == '/') {
        return 0;
    }
    w = snprintf(out, n, SEL_HVSC_HOST_DIR "%s", path);
    return w > 0 && (size_t)w < n;
}

static int url_of(const char* path, char* out, size_t n) {
    char host[1100];

    return sel_hvsc_host_path(path, host, sizeof host) && sel_url_path_encode(ROOT, host, out, n);
}

static int path_in(const char* url, char* path, size_t n) {
    char host[1100];
    const char* rel;

    if (!sel_url_path_decode(ROOT, url, host, sizeof host) ||
        strncmp(host, SEL_HVSC_HOST_DIR, sizeof SEL_HVSC_HOST_DIR - 1u) != 0) {
        return 0;
    }
    rel = host + sizeof SEL_HVSC_HOST_DIR - 1u;
    if (rel[0] == '\0' || strlen(rel) + 1u > n) {
        return 0;
    }
    memcpy(path, rel, strlen(rel) + 1u);
    return 1;
}

/* <MUSICIANS|GAMES|DEMOS>/<letter>/...: format from the top directory; the artist of a MUSICIANS file is
 * its directory two levels down ("Hubbard_Rob" -> "Hubbard Rob"); name = file without extension. */
int sel_hvsc_parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* fmt;
    const char* artist = "";
    size_t artist_len = 0, used = 0, len;
    const char* file;
    const char* dot;
    const char* p;
    char* f;
    char* a;
    char* nm;
    size_t i;

    if (path == NULL) {
        return 0;
    }
    if (strncmp(path, "MUSICIANS/", 10) == 0) {
        fmt = "HVSC - Musicians";
        p = strchr(path + 10, '/');
        if (p != NULL) {
            const char* e = strchr(p + 1, '/');
            if (e != NULL) {
                artist = p + 1;
                artist_len = (size_t)(e - artist);
            }
        }
    } else if (strncmp(path, "GAMES/", 6) == 0) {
        fmt = "HVSC - Games";
    } else if (strncmp(path, "DEMOS/", 6) == 0) {
        fmt = "HVSC - Demos";
    } else {
        return 0;
    }
    file = strrchr(path, '/') + 1;
    if (file[0] == '\0') {
        return 0;
    }
    f = scratch;
    len = strlen(fmt);
    if (used + len + 1u > n) {
        return 0;
    }
    memcpy(f, fmt, len + 1u);
    used += len + 1u;
    a = scratch + used;
    if (used + artist_len + 1u > n) {
        return 0;
    }
    for (i = 0; i < artist_len; i++) {
        a[i] = artist[i] == '_' ? ' ' : artist[i];
    }
    a[artist_len] = '\0';
    used += artist_len + 1u;
    nm = scratch + used;
    dot = strrchr(file, '.');
    len = dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file);
    if (used + len + 1u > n) {
        return 0;
    }
    memcpy(nm, file, len);
    nm[len] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)f;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)a;
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)nm;
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

/* LIBRARY_SYNC dispatch (I517): a URL runs the network sync, anything else reads a local mirror copy. */
static int hvsc_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    sel_hvsc_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (strncmp(arg, "http://", 7) == 0 || strncmp(arg, "https://", 8) == 0) {
        sel_hvsc_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_hvsc_net_sync(host, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_SONGLENGTHS_BYTES, nt.songlengths_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_STIL_BYTES, nt.stil_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_LISTING_DERIVED, nt.listing_derived);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_hvsc_sync(host, arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SONGLENGTHS, st.songlengths);
    sel_sync_stat(stats, cap, SEL_SYNC_STIL_ENTRIES, st.stil_entries);
    sel_sync_stat(stats, cap, SEL_SYNC_SUBTUNES, st.subtunes);
    return ok;
}

static const sel_source g_hvsc = {
    "hvsc", "HVSC", "https://hvsc.de/download/", "High Voltage SID Collection, served through Modland.", g_tags,
    NULL, sel_hvsc_parse, NULL, url_of, path_in, NULL, NULL, hvsc_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("hvsc", &g_hvsc);
}
