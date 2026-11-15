/* The `rko` source descriptor (row I1407): Remix.Kwed.org, https://remix.kwed.org/. A search source (SOURCE_SEARCH): one GET
 * of the UHC index HippoPlayer uses (shared client plugins/shared/source/uhc.c, searchtype `rkosearch`); the song key is
 * `listen/<id>/<file>.mp3` and the mp3 streams from https://uhc.remix.kwed.org/<key> (shared helper source/uhc_remix.c).
 * SELECTA_RKO_UHC replaces the UHC base (tests). */
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/uhc.h"
#include "source/uhc_remix.h"

#define SITE_HOME "https://remix.kwed.org/"
#define PLAY_BASE "https://uhc.remix.kwed.org/"
#define FORMAT_LABEL "Remix.Kwed.org"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int search(const sel_host* host, const char* query, sel_search_sink* out) {
    if (query == NULL || query[0] == '\0') {
        return -1;
    }
    return sel_uhc_search(host, "rko", "SELECTA_RKO_UHC", "rkosearch", query, sel_uhc_remix_row, out);
}

static int parse(const char* path, char* scratch, size_t n, sel_tag* out) {
    return sel_uhc_remix_parse(FORMAT_LABEL, path, scratch, n, out);
}

static int url_of(const char* path, char* out, size_t n) {
    return sel_uhc_remix_url(PLAY_BASE, path, out, n);
}

static int play_url(const sel_host* host, const char* path, char* out, size_t n) {
    (void)host;
    return url_of(path, out, n);
}

static const sel_source g_src = {
    "rko", "Remix.Kwed.org", SITE_HOME,
    "Remixes of C64 and Amiga music from Remix.Kwed.org; found by searching the site and streamed as mp3.",
    g_tags, NULL, parse, NULL, url_of, NULL, NULL, NULL, NULL, SEL_SOURCE_REMOTE | SEL_SOURCE_STREAMS,
    NULL, play_url,
    search,
    NULL, /* search_ready (I1037) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("rko", &g_src);
}
