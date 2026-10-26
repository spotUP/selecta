/* The `sndh` source descriptor (row I414; online-sources plan section 2; design doc 9.1): SNDH, the Atari ST's
 * YM2149 music. A path is relative to the /sndh/sndh_lf/ root: `Mad_Max/Wings_Of_Death.sndh`; the song key is
 * `sndh:<path>` and the canonical URL is https://sndh.atari.org/sndh/sndh_lf/<path>, percent-encoded. Tunes are filed
 * as Modland files them (bbmp sndh.py parse_path): the artist is the first folder as it is named, the name is the file
 * without its extension. */
#include "sndh_net.h"
#include "sndh_sync.h"

#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"
#include "source/url_path.h"

#include <string.h>

#define ROOT "https://sndh.atari.org/" SEL_SNDH_ROOT
#define FORMAT "SNDH"
#define UNKNOWN_ARTIST "- unknown"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    return sel_url_path_encode(ROOT, path, out, n);
}

static int path_in(const char* url, char* path, size_t n) {
    return sel_url_path_decode(ROOT, url, path, n);
}

int sel_sndh_parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* file;
    const char* dot;
    const char* slash;
    const char* artist = UNKNOWN_ARTIST;
    size_t artist_len = sizeof UNKNOWN_ARTIST - 1u, name_len, fmt_len = sizeof FORMAT - 1u;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    file = strrchr(path, '/');
    file = file != NULL ? file + 1 : path;
    if (file[0] == '\0') {
        return 0;
    }
    slash = strchr(path, '/');
    if (slash != NULL && slash != path) {
        artist = path;
        artist_len = (size_t)(slash - path);
    }
    dot = strrchr(file, '.');
    name_len = dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file);
    if (fmt_len + artist_len + name_len + 3u > n) {
        return 0;
    }
    memcpy(scratch, FORMAT, fmt_len + 1u);
    memcpy(scratch + fmt_len + 1u, artist, artist_len);
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
static int sndh_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    sel_sndh_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (strncmp(arg, "http://", 7) == 0 || strncmp(arg, "https://", 8) == 0) {
        sel_sndh_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_sndh_net_sync(host, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, nt.list_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_sndh_sync(host, arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    return ok;
}

static const sel_source g_sndh = {
    "sndh", "SNDH", "https://sndh.atari.org/", "Atari ST music: the YM2149 chip's SNDH archive.", g_tags,
    NULL, sel_sndh_parse, NULL, url_of, path_in, NULL, NULL, sndh_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("sndh", &g_sndh);
}
