/* The `amp` source descriptor (row I1407): AMP, Amiga Music Preservation, https://amp.dascene.net/. A search source
 * (SOURCE_SEARCH): one GET of the UHC index HippoPlayer uses (shared client plugins/shared/source/uhc.c, searchtype
 * `ampsearch`); the song key is `<index>/<name>` and the module is fetched one at a time, unpacked, from the hippoplayer.se
 * proxy (amp_row.c). SELECTA_AMP_UHC replaces the UHC base (tests). */
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/uhc.h"
#include "amp_row.h"

#define SITE_HOME "https://amp.dascene.net/"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int search(const sel_host* host, const char* query, sel_search_sink* out) {
    if (query == NULL || query[0] == '\0') {
        return -1;
    }
    return sel_uhc_search(host, "amp", "SELECTA_AMP_UHC", "ampsearch", query, sel_amp_row, out);
}

static int play_url(const sel_host* host, const char* path, char* out, size_t n) {
    (void)host;
    return sel_amp_url(path, out, n);
}

static const sel_source g_src = {
    "amp", "AMP", SITE_HOME,
    "Amiga Music Preservation: Amiga modules by author; found by searching the index and fetched one file at a time.",
    g_tags, NULL, sel_amp_parse, NULL, sel_amp_url, NULL, NULL, NULL, NULL, SEL_SOURCE_REMOTE,
    NULL, play_url,
    search,
    NULL, /* search_ready (I1037) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("amp", &g_src);
}
