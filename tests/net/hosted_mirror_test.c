/* Regression (I1218): a source hosted by another (hvsc, SEL_SRC_HOSTED_BY = modland) is fetched from the HOST's
 * mirrors under the host path. hvsc had its own built-in base https://hvsc.de/download/ plus the bare song path
 * (MUSICIANS/H/Huelsbeck_Chris/x.sid), which is 404 there (the tree sits under C64Music/), and on the Amiga route
 * <devilbox>/api/mirror/hvsc/<path>, a source the server does not know. This test loads the real plugin registry and
 * asserts the URL the mirror transport builds (sel_mirror_res.url of the attempt), unreachable mirrors so no network is
 * touched:
 *   host route   <SELECTA_MIRRORS_MODLAND base>pub/modules/HVSC/<percent-encoded path>
 *   amiga route  <devilbox>/api/mirror/modland/pub/modules/HVSC/<percent-encoded path>
 *   modland itself is unchanged. */
#include "net/mirrors.h"
#include "source.h"

#include <selecta/host.h>
#include <selecta/registry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sink(void* ctx, const void* data, size_t len) {
    (void)ctx;
    (void)data;
    (void)len;
    return 1;
}

static int check(const char* source, int amiga, const char* path, const char* want) {
    sel_mirror_set s;
    sel_mirror_res res;

    if (sel_mirrors_init(&s, source, amiga) == 0) {
        printf("FAIL %s amiga=%d: no mirror\n", source, amiga);
        return 1;
    }
    if (strcmp(s.source, source) != 0) {
        printf("FAIL %s: set source id changed to %s\n", source, s.source);
        return 1;
    }
    (void)sel_mirrors_fetch(&s, path, sink, NULL, NULL, &res);
    if (res.tries == 0 || strcmp(res.url, want) != 0) {
        printf("FAIL %s amiga=%d tries=%u\n  got  %s\n  want %s\n", source, amiga, res.tries, res.url, want);
        return 1;
    }
    return 0;
}

int main(void) {
    int bad = 0;
    const char* song = "MUSICIANS/H/Huelsbeck_Chris/Test Song (1).sid";
    const char* enc = "pub/modules/HVSC/MUSICIANS/H/Huelsbeck_Chris/Test%20Song%20%281%29.sid";
    char want[600];

    if (sel_registry_init(sel_host_default()) == 0) {
        printf("FAIL registry\n");
        return 1;
    }
    sel_source_set_plugins(sel_registry_count, sel_registry_get, sel_host_default());
    sel_source_register_builtin();
    if (sel_source_find("hvsc") == NULL) {
        printf("FAIL hvsc plugin not loaded\n");
        return 1;
    }
    setenv("SELECTA_NET_ALLOW", "127.0.0.1", 1);
    unsetenv("SELECTA_MIRROR");
    unsetenv("SELECTA_MIRRORS_HVSC");

    setenv("SELECTA_MIRRORS_MODLAND", "http://127.0.0.1:1/root/", 1);
    snprintf(want, sizeof want, "http://127.0.0.1:1/root/%s", enc);
    bad |= check("hvsc", 0, song, want);
    bad |= check("modland", 0, "pub/modules/TFMX/Chris Huelsbeck/mdat.l&m quick-mix",
                 "http://127.0.0.1:1/root/pub/modules/TFMX/Chris%20Huelsbeck/mdat.l%26m%20quick-mix");
    unsetenv("SELECTA_MIRRORS_MODLAND");

    setenv("SELECTA_DEVILBOX_URL", "http://127.0.0.1:1", 1);
    snprintf(want, sizeof want, "http://127.0.0.1:1/api/mirror/modland/%s", enc);
    bad |= check("hvsc", 1, song, want);
    bad |= check("modland", 1, "pub/modules/YM/Chris Huelsbeck/a.ym", "http://127.0.0.1:1/api/mirror/modland/pub/modules/YM/Chris%20Huelsbeck/a.ym");

    /* Default list (no env): the first built-in base of hvsc is the Modland mirror root, not hvsc.de/download. */
    {
        sel_mirror_set s;
        unsigned n = sel_mirrors_init(&s, "hvsc", 0);

        if (n == 0 || strstr(s.base[0], "hvsc.de") != NULL) {
            printf("FAIL builtin hvsc list n=%u first=%s\n", n, n ? s.base[0] : "-");
            bad = 1;
        }
    }
    if (bad) {
        return 1;
    }
    printf("hosted_mirror_ok\n");
    return 0;
}
