/* I211b: the cache and DEViLBOX paths take their remote-source decision from the source table, so every registered
 * remote source (modland, hvsc, hippo, ...) works and local keys, plain paths and URLs do not. */
#include "../../src/library/cache.h"
#include "../../src/library/net/devilbox.h"
#include "../../src/library/source.h"

#include <stdio.h>
#include <string.h>

static int g_bad;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        g_bad++;
    }
}

int main(void) {
    static const char* const remote[] = {"modland:p/purple.xm", "hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid",
                                         "hippo:LMan_SID_Chip_Club.prg"};
    static const char* const not_remote[] = {"local:selecta-test.mod", "selecta-test.mod", "nosuch:x/y.mod",
                                             "http://x.test/a.mp3", "modland:", "radio:station"};
    char source[16];
    const char* path = NULL;
    unsigned i;

    for (i = 0; i < sizeof remote / sizeof remote[0]; i++) {
        char what[96];

        snprintf(what, sizeof what, "cache remote %s", remote[i]);
        check(sel_cache_is_remote(remote[i]), what);
        snprintf(what, sizeof what, "devilbox item %s", remote[i]);
        check(sel_devilbox_item(remote[i], source, sizeof source, &path), what);
    }
    check(sel_devilbox_item("hippo:a.prg", source, sizeof source, &path) && strcmp(source, "hippo") == 0 &&
              strcmp(path, "a.prg") == 0,
          "hippo source and item split");
    for (i = 0; i < sizeof not_remote / sizeof not_remote[0]; i++) {
        char what[96];

        snprintf(what, sizeof what, "cache not remote %s", not_remote[i]);
        check(!sel_cache_is_remote(not_remote[i]), what);
        snprintf(what, sizeof what, "devilbox not item %s", not_remote[i]);
        check(!sel_devilbox_item(not_remote[i], source, sizeof source, &path), what);
    }
    if (g_bad == 0) printf("key_remote_ok\n");
    return g_bad != 0;
}
