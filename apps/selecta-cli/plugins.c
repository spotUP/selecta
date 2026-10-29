/* plugins [--lint] [--owner <format>]: lists the static registry; --lint walks every
 * descriptor and fails on unknown or duplicate tags (T5). `--machine <name>` lists what a fake
 * machine (sel_machine_fake) can run, `--stats` prints the registry filter counters (T49). */
#include <selecta/machine.h>
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
    uint32_t i, n;
    const sel_tag* machine = NULL;
    int stats = 0;
    int a;

    for (a = 0; a < argc; a++) {
        if (strcmp(argv[a], "--machine") == 0 && a + 1 < argc) {
            machine = sel_machine_fake(argv[++a]);
            if (machine == NULL) {
                fprintf(stderr, "unknown machine %s (a500, a1200, a1200-fpu, a060)\n", argv[a]);
                return 1;
            }
        } else if (strcmp(argv[a], "--stats") == 0) {
            stats = 1;
        }
    }
    n = sel_registry_init_machine(sel_host_default(), machine);
    if (argc > 0 && strcmp(argv[0], "--lint") == 0) {
        sel_lint_report r;
        sel_registry_lint(&r);
        printf("plugins=%u unknown_tags=%u duplicate_tags=%u\n", (unsigned)r.plugins,
               (unsigned)r.unknown_tags, (unsigned)r.duplicate_tags);
        return (r.unknown_tags || r.duplicate_tags) ? 1 : 0;
    }
    if (argc > 1 && strcmp(argv[0], "--owner") == 0) {
        const sel_tag* o = sel_registry_format_owner(argv[1]);
        printf("owner=%s\n", o != NULL ? (const char*)sel_tag_find(o, SEL_PLUGIN_ID, (uintptr_t) "?") : "none");
        return 0;
    }
    if (argc > 1 && strcmp(argv[0], "--formats") == 0) {
        return formats(argv[1], n);
    }
    for (i = 0; i < n; i++) {
        const sel_tag* d = sel_registry_get(i);
        if (sel_tag_find(d, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_UI && !sel_registry_ui_listed(d)) {
            continue; /* a UI whose system library is absent is registered but not listed (MUI-P1-6) */
        }
        printf("%s kind=%u version=%u origin=%s\n", (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "?"),
               (unsigned)sel_tag_find(d, SEL_PLUGIN_KIND, 0),
               (unsigned)sel_tag_find(d, SEL_PLUGIN_VERSION, 0), sel_registry_origin(i));
    }
    if (stats) {
        const sel_registry_stats* st = sel_registry_get_stats();
        printf("registry_total=%u\nfiltered=%u\nhidden_min_cpu=%u\nhidden_needs_float=%u\n"
               "hidden_memory=%u\nhidden_chip=%u\ndyn_loaded=%u\ndyn_rejected=%u\n",
               (unsigned)st->total, (unsigned)st->filtered, (unsigned)st->hidden_min_cpu,
               (unsigned)st->hidden_needs_float, (unsigned)st->hidden_memory,
               (unsigned)st->hidden_chip, (unsigned)st->dyn_loaded, (unsigned)st->dyn_rejected);
    }
    return 0;
}
