/* plugins [--lint]: lists the static registry; --lint walks every
 * descriptor and fails on unknown or duplicate tags (T5). */
#include <selecta/registry.h>

#include "cli.h"

#include <stdio.h>
#include <string.h>

int sel_cli_plugins(int argc, char** argv) {
    uint32_t i, n = sel_registry_init(sel_host_default());
    if (argc > 0 && strcmp(argv[0], "--lint") == 0) {
        sel_lint_report r;
        sel_registry_lint(&r);
        printf("plugins=%u unknown_tags=%u duplicate_tags=%u\n", (unsigned)r.plugins,
               (unsigned)r.unknown_tags, (unsigned)r.duplicate_tags);
        return (r.unknown_tags || r.duplicate_tags) ? 1 : 0;
    }
    for (i = 0; i < n; i++) {
        const sel_tag* d = sel_registry_get(i);
        printf("%s kind=%u version=%u\n", (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "?"),
               (unsigned)sel_tag_find(d, SEL_PLUGIN_KIND, 0),
               (unsigned)sel_tag_find(d, SEL_PLUGIN_VERSION, 0));
    }
    return 0;
}
