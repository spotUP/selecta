/* plugins [--lint]: lists the static registry; --lint walks every
 * descriptor and fails on unknown or duplicate tags (T5). */
#include <selecta/registry.h>

#include "cli.h"

#include <stdio.h>
#include <string.h>

/* --formats <plugin>: the plugin's SEL_RP_FORMATS list (comma separated ids)
 * as "<plugin>_players=N" and one "player <id>" line each. */
static int formats(const char* plugin, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        const sel_tag* d = sel_registry_get(i);
        const char* id = (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "");
        const sel_tag* hooks;
        const char* list;
        unsigned n = 0;
        if (strcmp(id, plugin) != 0) {
            continue;
        }
        hooks = (const sel_tag*)sel_tag_find(d, SEL_PLUGIN_TAGS, 0);
        list = hooks != NULL ? (const char*)sel_tag_find(hooks, SEL_RP_FORMATS, 0) : NULL;
        if (list == NULL) {
            fprintf(stderr, "plugin %s reports no formats\n", plugin);
            return 1;
        }
        while (*list != '\0') {
            const char* e = strchr(list, ',');
            size_t len = e != NULL ? (size_t)(e - list) : strlen(list);
            n++;
            list += len + (e != NULL ? 1u : 0u);
        }
        printf("%s_players=%u\n", plugin, n);
        list = (const char*)sel_tag_find(hooks, SEL_RP_FORMATS, 0);
        while (*list != '\0') {
            const char* e = strchr(list, ',');
            size_t len = e != NULL ? (size_t)(e - list) : strlen(list);
            printf("player %.*s\n", (int)len, list);
            list += len + (e != NULL ? 1u : 0u);
        }
        return 0;
    }
    fprintf(stderr, "no plugin named %s\n", plugin);
    return 1;
}

int sel_cli_plugins(int argc, char** argv) {
    uint32_t i, n = sel_registry_init(sel_host_default());
    if (argc > 0 && strcmp(argv[0], "--lint") == 0) {
        sel_lint_report r;
        sel_registry_lint(&r);
        printf("plugins=%u unknown_tags=%u duplicate_tags=%u\n", (unsigned)r.plugins,
               (unsigned)r.unknown_tags, (unsigned)r.duplicate_tags);
        return (r.unknown_tags || r.duplicate_tags) ? 1 : 0;
    }
    if (argc > 1 && strcmp(argv[0], "--formats") == 0) {
        return formats(argv[1], n);
    }
    for (i = 0; i < n; i++) {
        const sel_tag* d = sel_registry_get(i);
        printf("%s kind=%u version=%u\n", (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "?"),
               (unsigned)sel_tag_find(d, SEL_PLUGIN_KIND, 0),
               (unsigned)sel_tag_find(d, SEL_PLUGIN_VERSION, 0));
    }
    return 0;
}
