/* `selecta-cli support list|get <id|all>|path <file> --cache-dir <dir> [--config-dir <dir>]` (I751): drives the
 * support-file downloader (src/library/support.h) the way the host does. Consent is the pref
 * library.support_downloads in <config dir>/prefs.conf (default off). Lines are `name=value`.
 * get: exit 0 when every requested file is present and verified, 1 otherwise; 2 on usage.
 * path: prints `path=<file>` and exits 0 when present and valid, else exits 1. */
#include "cli.h"
#include "support.h"

#include "../../src/engine/prefs.h"

#include <stdio.h>
#include <string.h>

int sel_cli_support(int argc, char** argv) {
    const char* cache = NULL;
    const char* cfg = NULL;
    const char* cmd = argc > 0 ? argv[0] : NULL;
    const char* arg = argc > 1 ? argv[1] : NULL;
    sel_support_entry m[SEL_SUPPORT_MAX];
    unsigned n, i;
    int i2, rc = 0, found = 0, consent = 0;
    sel_prefs prefs;
    for (i2 = (cmd != NULL && strcmp(cmd, "list") == 0) ? 1 : 2; i2 + 1 < argc; i2 += 2) {
        if (strcmp(argv[i2], "--cache-dir") == 0) {
            cache = argv[i2 + 1];
        } else if (strcmp(argv[i2], "--config-dir") == 0) {
            cfg = argv[i2 + 1];
        }
    }
    if (cmd == NULL || cache == NULL || (strcmp(cmd, "list") != 0 && arg == NULL)) {
        fprintf(stderr, "usage: support list|get <id|all>|path <file> --cache-dir <dir> [--config-dir <dir>]\n");
        return 2;
    }
    sel_prefs_init(&prefs);
    if (cfg != NULL) {
        sel_prefs_load(&prefs, cfg);
    }
    consent = strcmp(prefs.value[SEL_PREF_LIBRARY_SUPPORT], "on") == 0;
    n = sel_support_manifest(m, SEL_SUPPORT_MAX);
    printf("manifest_entries=%u\nconsent=%d\n", n, consent);
    if (strcmp(cmd, "path") == 0) {
        char p[1100];
        if (sel_support_path(cache, arg, p, sizeof p)) {
            printf("path=%s\n", p);
            return 0;
        }
        printf("path=\n");
        return 1;
    }
    for (i = 0; i < n; i++) {
        char p[1100];
        int have = sel_support_path(cache, m[i].file, p, sizeof p);
        if (strcmp(cmd, "list") == 0) {
            printf("support %s file=%s plugin=%s size=%lu present=%d licence=%s\n", m[i].id, m[i].file, m[i].plugin,
                   m[i].cut_len != 0 ? m[i].cut_len : m[i].size, have, m[i].licence);
        } else if (strcmp(cmd, "get") == 0 && (strcmp(arg, "all") == 0 || strcmp(arg, m[i].id) == 0)) {
            sel_support_result r = sel_support_get(cache, &m[i], consent);
            found++;
            printf("get %s file=%s result=%s\n", m[i].id, m[i].file, sel_support_result_name(r));
            rc |= r != SEL_SUPPORT_OK;
        }
    }
    if (strcmp(cmd, "get") == 0 && found == 0) {
        printf("get %s result=%s\n", arg, sel_support_result_name(SEL_SUPPORT_UNKNOWN));
        rc = 1;
    }
    return rc;
}
