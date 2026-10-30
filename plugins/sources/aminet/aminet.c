/* The `aminet` source descriptor (row I405; online-sources plan section 5.1, D-OS-6): module archives of the Aminet
 * `mods/` hierarchy. A path is relative to the site root and decoded: `mods/4mat/AnarchyMenu10.lha`; the song key is
 * `aminet:<path>` and the canonical URL is https://aminet.net/<path>, percent-encoded. The file is an archive; the
 * engine unpacks the module at playback. */
#include "aminet_net.h"
#include "aminet_sync.h"

#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"
#include "source/url_path.h"

#include <stdio.h>
#include <string.h>

#define ROOT "https://aminet.net/"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    return sel_url_path_encode(ROOT, path, out, n);
}

static int path_in(const char* url, char* path, size_t n) {
    return sel_url_path_decode(ROOT, url, path, n);
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    sel_aminet_row row;
    char line[SEL_AMINET_PATH_MAX + 64u];
    size_t fmt_len = sizeof SEL_AMINET_FORMAT;
    size_t artist_len, name_len;

    if (path == NULL || path[0] == '\0' || strlen(path) + 40u > sizeof line) {
        return 0;
    }
    /* Reuse the INDEX line parser: a synthetic line carries the path only, so the artist is the directory leaf. */
    {
        const char* file = strrchr(path, '/');
        const char* leaf = path;
        const char* p;
        size_t dir_len;

        if (file == NULL) {
            return 0;
        }
        dir_len = (size_t)(file - path);
        for (p = path; p < file; p++) {
            if (*p == '/') {
                leaf = p + 1;
            }
        }
        snprintf(line, sizeof line, "%s %.*s 0K 0 by %.*s  ", file + 1, (int)dir_len, path, (int)(file - leaf), leaf);
    }
    if (!sel_aminet_parse_line(line, &row)) {
        return 0;
    }
    artist_len = strlen(row.artist) + 1u;
    name_len = strlen(row.name) + 1u;
    if (fmt_len + artist_len + name_len > n) {
        return 0;
    }
    memcpy(scratch, SEL_AMINET_FORMAT, fmt_len);
    memcpy(scratch + fmt_len, row.artist, artist_len);
    memcpy(scratch + fmt_len + artist_len, row.name, name_len);
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + fmt_len);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + fmt_len + artist_len);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

/* LIBRARY_SYNC dispatch: a URL runs the network sync, anything else reads a local mirror copy. */
static int aminet_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    sel_aminet_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (strncmp(arg, "http://", 7) == 0 || strncmp(arg, "https://", 8) == 0) {
        sel_aminet_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_aminet_net_sync(host, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, nt.list_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_aminet_sync(host, arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    return ok;
}

static const sel_source g_aminet = {
    "aminet", "Aminet", "https://aminet.net/",
    "Amiga module archives from the Aminet mods directories; each LHA archive unpacks to a tracker module.", g_tags,
    NULL, parse, NULL, url_of, path_in, NULL, NULL, aminet_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL, NULL,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("aminet", &g_aminet);
}
