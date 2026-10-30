/* Reachability probe for the source plugin path (I517 phase 3; plan 2026-10-26-source-plugins.md, Reachability). The
 * only thing core knows about this source is the registry entry sel_add_plugin gives it. Its sync writes three songs
 * through the catalogue service and counts the call in a sentinel the test reads. */
#include <selecta/catalogue.h>
#include <selecta/plugin.h>
#include <selecta/source.h>
#include "source/descriptor.h"

#include <stdlib.h>
#include <string.h>

unsigned sel_testsrc_sync_calls;

static int testsrc_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    static const char* const names[3] = {"one", "two", "three"};
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    unsigned i;

    (void)arg;
    (void)now;
    (void)cap;
    sel_testsrc_sync_calls++;
    if (getenv("SELECTA_TESTSRC_FAIL") != NULL) { /* I582: the dui-sync-banner test makes this source fail like a source without network */
        return 0;
    }
    if (cat == NULL || !cat->begin(host, "testsrc")) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        char path[16];
        sel_catalogue_song s;

        memset(&s, 0, sizeof s);
        path[0] = (char)('a' + i);
        memcpy(path + 1, "/x.mod", 7);
        s.path = path;
        s.format = "ProTracker";
        s.artist = "Probe";
        s.album = s.md5 = "";
        s.name = names[i];
        if (!cat->song(host, &s)) {
            cat->abort(host);
            return 0;
        }
    }
    stats[0].tag = SEL_SYNC_LISTING_ROWS;
    stats[0].value = 3;
    stats[1].tag = SEL_TAG_END;
    stats[1].value = 0;
    return cat->commit(host);
}

static const sel_source g_testsrc = {
    "testsrc", "Test Source", "https://example.invalid/", "Reachability probe for source plugins.", NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, testsrc_sync, SEL_SOURCE_REMOTE | SEL_SOURCE_SYNCS, NULL, NULL,
};

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    return sel_source_plugin_descriptor("testsrc", &g_testsrc);
}
