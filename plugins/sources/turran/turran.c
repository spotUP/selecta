/* The `turran` source descriptor (row I1407): the Turran FTP archive, https://uhc.megaburken.net/turran/. A search source
 * (SOURCE_SEARCH): one GET of the UHC index HippoPlayer uses (shared client plugins/shared/source/uhc.c, searchtype
 * `turransearch`). HippoPlayer prefixes every query with "collection/music/" so only the music tree answers; so do we. The
 * song key is `<path>/<file>` (turran_row.c keeps playable extensions only); the file is fetched through the mirror list
 * ("turran" in src/library/net/mirrors.c, https://uhc.megaburken.net/turran/). SELECTA_TURRAN_UHC replaces the UHC base (tests). */
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"
#include "source/uhc.h"
#include "turran_row.h"

#include <stdio.h>

#define SITE_HOME "https://uhc.megaburken.net/turran/"

static const sel_tag g_tags[] = {
    {SEL_TAG_END, 0},
};

static int search(const sel_host* host, const char* query, sel_search_sink* out) {
    char words[160];

    if (query == NULL || query[0] == '\0' || (size_t)snprintf(words, sizeof words, "collection/music/ %s", query) >= sizeof words) {
        return -1;
    }
    return sel_uhc_search(host, "turran", "SELECTA_TURRAN_UHC", "turransearch", words, sel_turran_row, out);
}

static const sel_source g_src = {
    "turran", "Turran FTP", SITE_HOME,
    "The music tree of the Turran FTP Amiga archive; found by searching the index and fetched one file at a time.",
    g_tags, NULL, sel_turran_parse, NULL, NULL, NULL, NULL, NULL, NULL, SEL_SOURCE_REMOTE,
    NULL, NULL,
    search,
    NULL, /* search_ready (I1037) */
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("turran", &g_src);
}
