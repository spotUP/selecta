/* The `modland` source descriptor (plan Phase 4, I157; design doc 9.1): path structure, canonical URL
 * and its inverse. The catalogue listing and transports arrive with the network phases (I162). The
 * path is `pub/modules/<format>/<artist>/<file>` relative to the Modland root, so the song key is
 * `modland:pub/modules/<format>/<artist>/<file>`. */
#include "../source.h"
#include "modland_net.h"
#include "sync_host.h"

#include <stdio.h>
#include <string.h>

#define ROOT "https://modland.com/"
#define MODULES "pub/modules/"

static const sel_tag g_tags[] = {
    {SEL_SRC_HAS_LISTS, 1},
    {SEL_SRC_HAS_MD5, 1},
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    size_t used = 0;
    static const char hex[] = "0123456789ABCDEF";

    if (path == NULL || strncmp(path, MODULES, sizeof MODULES - 1u) != 0 || n < sizeof ROOT) {
        return 0;
    }
    memcpy(out, ROOT, sizeof ROOT - 1u);
    used = sizeof ROOT - 1u;
    for (; *path != '\0'; path++) {
        unsigned char c = (unsigned char)*path;
        int plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' ||
                    c == '-' || c == '_' || c == '.' || c == '~';
        if (used + (plain ? 1u : 3u) + 1u > n) {
            return 0;
        }
        if (plain) {
            out[used++] = (char)c;
        } else {
            out[used++] = '%';
            out[used++] = hex[c >> 4];
            out[used++] = hex[c & 15u];
        }
    }
    out[used] = '\0';
    return 1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static int path_in(const char* url, char* path, size_t n) {
    size_t used = 0;

    if (url == NULL || strncmp(url, ROOT, sizeof ROOT - 1u) != 0) {
        return 0;
    }
    url += sizeof ROOT - 1u;
    for (; *url != '\0'; url++) {
        char c = *url;
        if (c == '%') {
            int hi = hex_value(url[1]);
            int lo = hi >= 0 ? hex_value(url[2]) : -1;
            if (lo < 0) {
                return 0;
            }
            c = (char)(hi * 16 + lo);
            url += 2;
        }
        if (used + 1u >= n) {
            return 0;
        }
        path[used++] = c;
    }
    path[used] = '\0';
    return strncmp(path, MODULES, sizeof MODULES - 1u) == 0;
}

/* pub/modules/<format>/<artist>/<file>: copies format, artist and the file name (without extension)
 * into scratch. */
static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    const char* fmt;
    const char* slash;
    const char* artist;
    const char* file;
    const char* dot;
    size_t used = 0, len;
    char* f;
    char* a;
    char* nm;

    if (path == NULL || strncmp(path, MODULES, sizeof MODULES - 1u) != 0) {
        return 0;
    }
    fmt = path + sizeof MODULES - 1u;
    slash = strchr(fmt, '/');
    if (slash == NULL) {
        return 0;
    }
    artist = slash + 1;
    slash = strchr(artist, '/');
    if (slash == NULL) {
        return 0;
    }
    file = strrchr(path, '/') + 1;
    if (file[0] == '\0') {
        return 0;
    }
    f = scratch + used;
    len = (size_t)(strchr(fmt, '/') - fmt);
    if (used + len + 1u > n) {
        return 0;
    }
    memcpy(f, fmt, len);
    f[len] = '\0';
    used += len + 1u;
    a = scratch + used;
    len = (size_t)(slash - artist);
    if (used + len + 1u > n) {
        return 0;
    }
    memcpy(a, artist, len);
    a[len] = '\0';
    used += len + 1u;
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

/* LIBRARY_SYNC dispatch (I517): a URL runs the network sync, anything else applies a local mirror copy. */
static int modland_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_local_scan_config* cfg = sel_sync_cfg(host);
    sel_modland_sync_stats st;
    int ok;

    memset(&st, 0, sizeof st);
    if (sel_modland_is_url(arg)) {
        sel_modland_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_modland_net_sync(cfg, arg, now, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_ZIP_BYTES, nt.zip_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, nt.listing_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_UNPACK_PEAK_HEAP, nt.unpack_peak_heap);
        sel_sync_stat(stats, cap, SEL_SYNC_CHANGELOGS_FETCHED, nt.changelogs_fetched);
        sel_sync_stat(stats, cap, SEL_SYNC_CHANGELOGS_MISSING, nt.changelogs_missing);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_modland_sync(cfg, arg, now, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_CHANGELOGS, st.changelogs);
    sel_sync_stat(stats, cap, SEL_SYNC_CL_ADDED, st.cl_added);
    sel_sync_stat(stats, cap, SEL_SYNC_CL_REMOVED, st.cl_removed);
    sel_sync_stat(stats, cap, SEL_SYNC_CL_MOVED, st.cl_moved);
    sel_sync_stat(stats, cap, SEL_SYNC_CL_UPDATED, st.cl_updated);
    sel_sync_stat(stats, cap, SEL_SYNC_NEW_THIS_WEEK, st.new_this_week);
    return ok;
}

static const sel_source g_modland = {
    "modland", "Modland", "https://modland.com/", "The largest tracker module archive.", g_tags,
    NULL, parse, NULL, url_of, path_in, NULL, NULL, modland_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS | SEL_SOURCE_NEW_THIS_WEEK, NULL,
};

const sel_source* sel_source_modland(void) {
    return &g_modland;
}
