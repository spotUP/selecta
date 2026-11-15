/* The `amigaremix` source descriptor (row I1407): AmigaRemix, https://amigaremix.com/. A search source (SOURCE_SEARCH): one GET
 * of the UHC index HippoPlayer uses (shared client plugins/shared/source/uhc.c, searchtype `amigaremixsearch`); the song key
 * is `listen/<id>/<file>.mp3` and the mp3 streams from https://uhc.amigaremix.com/<key> (shared helper source/uhc_remix.c).
 * SELECTA_AMIGAREMIX_UHC replaces the UHC base (tests). */
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/uhc.h"
#include "source/uhc_remix.h"

#define SITE_HOME "https://amigaremix.com/"
#define PLAY_BASE "https://uhc.amigaremix.com/"
#define FORMAT_LABEL "AmigaRemix"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int search(const sel_host* host, const char* query, sel_search_sink* out) {
    if (query == NULL || query[0] == '\0') {
        return -1;
    }
    return sel_uhc_search(host, "amigaremix", "SELECTA_AMIGAREMIX_UHC", "amigaremixsearch", query, sel_uhc_remix_row, out);
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
    "amigaremix", "AmigaRemix", SITE_HOME,
    "Remixes of Amiga game and chip music from AmigaRemix; found by searching the site and streamed as mp3.",
    g_tags, NULL, parse, NULL, url_of, NULL, NULL, NULL, NULL, SEL_SOURCE_REMOTE | SEL_SOURCE_STREAMS,
    NULL, play_url,
    search,
    NULL, /* search_ready (I1037) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("amigaremix", &g_src);
}
