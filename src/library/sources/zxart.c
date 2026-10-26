/* The `zxart` source descriptor (row I415; online-sources plan section 2; design doc 9.1): ZX-Art, the ZX Spectrum's
 * music. A path is `<id>/<file name>` (`19636/MmcM_-_Beyond_Time_(2013).mt3`); the song key is `zxart:<path>` and
 * the canonical URL is https://zxart.ee/file/id:<id>/filename:<name>, the name percent-encoded. The format and artist
 * come from the site's list, not from the path, so parse gives bbmp's fallback ("Spectrum", "- unknown"). */
#include "../source.h"
#include "zxart_net.h"
#include "modland_net.h"
#include "sync_host.h"
#include "source/url_path.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SITE "https://zxart.ee/file/id:"
#define NAME_KEY "/filename:"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int url_of(const char* path, char* out, size_t n) {
    char root[64];
    const char* slash = strchr(path, '/');
    size_t id_len;

    if (slash == NULL || slash == path || slash[1] == '\0') {
        return 0;
    }
    id_len = (size_t)(slash - path);
    if (id_len > 20u) {
        return 0;
    }
    snprintf(root, sizeof root, SITE "%.*s" NAME_KEY, (int)id_len, path);
    return sel_url_path_encode(root, slash + 1, out, n);
}

static int path_in(const char* url, char* path, size_t n) {
    const char* id;
    const char* slash;
    size_t id_len, i;

    if (url == NULL || strncmp(url, SITE, sizeof SITE - 1u) != 0) {
        return 0;
    }
    id = url + sizeof SITE - 1u;
    slash = strchr(id, '/');
    if (slash == NULL || strncmp(slash, NAME_KEY, sizeof NAME_KEY - 1u) != 0) {
        return 0;
    }
    id_len = (size_t)(slash - id);
    if (id_len == 0 || id_len + 2u > n) {
        return 0;
    }
    for (i = 0; i < id_len; i++) {
        if (!isdigit((unsigned char)id[i])) {
            return 0;
        }
    }
    memcpy(path, id, id_len);
    path[id_len] = '/';
    if (slash[sizeof NAME_KEY - 1u] == '\0') {
        return 0;
    }
    return sel_url_unquote(slash + sizeof NAME_KEY - 1u, path + id_len + 1u, n - id_len - 1u);
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    static const char fmt[] = "Spectrum";
    static const char artist[] = "- unknown";
    const char* file;
    size_t name_len;

    if (path == NULL || (file = strchr(path, '/')) == NULL || file[1] == '\0') {
        return 0;
    }
    file++;
    name_len = strlen(file);
    if (sizeof fmt + sizeof artist + name_len + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, sizeof fmt);
    memcpy(scratch + sizeof fmt, artist, sizeof artist);
    memcpy(scratch + sizeof fmt + sizeof artist, file, name_len + 1u);
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)(scratch + sizeof fmt);
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(scratch + sizeof fmt + sizeof artist);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

/* LIBRARY_SYNC dispatch (I517): a URL runs the network sync, anything else reads a local mirror copy. */
static int zxart_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    const sel_local_scan_config* cfg = sel_sync_cfg(host);
    sel_zxart_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (sel_modland_is_url(arg)) {
        sel_zxart_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_zxart_net_sync(cfg, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, nt.list_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_zxart_sync(cfg, arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    return ok;
}

static const sel_source g_zxart = {
    "zxart", "ZX-Art", "https://zxart.ee/", "ZX Spectrum music: AY trackers, beeper and digital, from zxart.ee.",
    g_tags, NULL, parse, NULL, url_of, path_in, NULL, NULL, zxart_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL,
};

const sel_source* sel_source_zxart(void) {
    return &g_zxart;
}
