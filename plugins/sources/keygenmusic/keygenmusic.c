/* The `keygenmusic` source descriptor (row I416; online-sources plan section 2; design doc 9.1): the music of
 * keygens, cracks and trainers from keygenmusic.tk. A path is relative to the /kgm/ root and decoded:
 * `TSRh/TSRh - Opus Creator 9.01 kg.xm`; the song key is `keygenmusic:<path>` and the canonical URL is
 * https://keygenmusic.tk/kgm/<path>, percent-encoded. Format from the extension, artist the release group (bbmp
 * keygenmusic.py parse), name the file without its extension (the sync uses what was cracked instead). */
#include "keygenmusic_net.h"
#include "keygenmusic_sync.h"

#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/sync_stat.h"
#include "source/url_path.h"

#include <string.h>

#define ROOT "https://keygenmusic.tk/" SEL_KEYGEN_ROOT

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
    const char* fmt;
    const char* file;
    const char* dot;
    size_t fmt_len, name_len;
    char* artist;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    fmt = sel_keygen_format(path);
    fmt_len = strlen(fmt);
    file = strrchr(path, '/');
    file = file != NULL ? file + 1 : path;
    dot = strrchr(file, '.');
    name_len = dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file);
    if (fmt_len + 1u + SEL_KEYGEN_TEXT_MAX + name_len + 1u > n) {
        return 0;
    }
    memcpy(scratch, fmt, fmt_len + 1u);
    artist = scratch + fmt_len + 1u;
    if (!sel_keygen_group(path, "", artist, SEL_KEYGEN_TEXT_MAX)) {
        return 0;
    }
    memcpy(artist + strlen(artist) + 1u, file, name_len);
    artist[strlen(artist) + 1u + name_len] = '\0';
    out[0].tag = SEL_SRC_FORMAT;
    out[0].value = (uintptr_t)scratch;
    out[1].tag = SEL_SRC_ARTIST;
    out[1].value = (uintptr_t)artist;
    out[2].tag = SEL_SRC_NAME;
    out[2].value = (uintptr_t)(artist + strlen(artist) + 1u);
    out[3].tag = SEL_TAG_END;
    out[3].value = 0;
    return 1;
}

/* LIBRARY_SYNC dispatch (I517): a URL runs the network sync, anything else reads a local mirror copy. */
static int keygenmusic_sync_host(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    sel_keygen_sync_stats st;
    int ok;

    (void)now;
    memset(&st, 0, sizeof st);
    if (strncmp(arg, "http://", 7) == 0 || strncmp(arg, "https://", 8) == 0) {
        sel_keygen_net_stats nt;

        memset(&nt, 0, sizeof nt);
        ok = sel_keygen_net_sync(host, arg, &st, &nt);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_BYTES, nt.list_bytes);
        sel_sync_stat(stats, cap, SEL_SYNC_NET_REQUESTS, nt.http_requests);
    } else {
        ok = sel_keygen_sync(host, arg, &st);
    }
    sel_sync_stat(stats, cap, SEL_SYNC_LISTING_ROWS, st.listing_rows);
    sel_sync_stat(stats, cap, SEL_SYNC_SKIPPED_ROWS, st.skipped_rows);
    return ok;
}

static const sel_source g_keygenmusic = {
    "keygenmusic", "Keygen Music", "https://keygenmusic.tk/",
    "The music of keygens, cracks and trainers: tracker modules from release groups.", g_tags,
    NULL, parse, NULL, url_of, path_in, NULL, NULL, keygenmusic_sync_host, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("keygenmusic", &g_keygenmusic);
}
