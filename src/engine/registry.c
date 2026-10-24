#include <selecta/machine.h>
#include <selecta/registry.h>

#include <stdio.h>
#include <string.h>

/* Generated into the build tree by cmake/selecta_registry.cmake. */
extern const sel_plugin_entry_fn sel_registry_entries[];
extern const uint32_t sel_registry_entry_count;

#define SEL_REGISTRY_MAX 64

static const sel_host* g_host;
static const sel_tag* g_desc[SEL_REGISTRY_MAX];
static uint32_t g_count;
static sel_registry_stats g_stats;

/* Machine CPU as the number a plugin declares in SEL_RP_MIN_CPU (68020 = 68020). */
static uint32_t cpu_number(uintptr_t cpu) {
    switch (cpu) {
    case SEL_CPU_68000:
        return 68000u;
    case SEL_CPU_68010:
        return 68010u;
    case SEL_CPU_68020:
        return 68020u;
    case SEL_CPU_68030:
        return 68030u;
    case SEL_CPU_68040:
        return 68040u;
    case SEL_CPU_68060:
        return 68060u;
    case SEL_CPU_DESKTOP:
        return 0xFFFFFFFFu;
    default:
        return 0u;
    }
}

/* The one machine filter (desktop and 68k). Returns 1 when `d` runs on `machine` and counts the
 * first unmet need per reason in g_stats; a plugin failing several needs counts under each. */
static int fits(const sel_tag* d, const sel_tag* machine) {
    const sel_tag* hooks = (const sel_tag*)sel_tag_find(d, SEL_PLUGIN_TAGS, 0);
    uintptr_t min_cpu = hooks ? sel_tag_find(hooks, SEL_RP_MIN_CPU, 0) : 0;
    uintptr_t floats = hooks ? sel_tag_find(hooks, SEL_RP_NEEDS_FLOAT, 0) : 0;
    uintptr_t memory = hooks ? sel_tag_find(hooks, SEL_RP_MEMORY, 0) : 0;
    uintptr_t chip = hooks ? sel_tag_find(hooks, SEL_OUT_MEM_CHIP, 0) : 0;
    int ok = 1;

    if (machine == NULL) {
        return 1;
    }
    if (min_cpu > cpu_number(sel_tag_find(machine, SEL_MACHINE_CPU, 0))) {
        g_stats.hidden_min_cpu++;
        ok = 0;
    }
    if (floats && !sel_tag_find(machine, SEL_MACHINE_FPU, 0)) {
        g_stats.hidden_needs_float++;
        ok = 0;
    }
    if (memory > sel_tag_find(machine, SEL_MACHINE_CHIP_RAM, 0) +
                     sel_tag_find(machine, SEL_MACHINE_FAST_RAM, 0)) {
        g_stats.hidden_memory++;
        ok = 0;
    }
    if (chip > sel_tag_find(machine, SEL_MACHINE_CHIP_RAM, 0)) {
        g_stats.hidden_chip++;
        ok = 0;
    }
    return ok;
}

uint32_t sel_registry_init_machine(const sel_host* host, const sel_tag* machine) {
    uint32_t i;
    g_host = host;
    g_count = 0;
    memset(&g_stats, 0, sizeof g_stats);
    for (i = 0; i < sel_registry_entry_count; i++) {
        const sel_tag* d = sel_registry_entries[i](SEL_API_VERSION);
        if (!d) {
            continue;
        }
        g_stats.total++;
        if (!fits(d, machine)) {
            g_stats.filtered++;
        } else if (g_count < SEL_REGISTRY_MAX) {
            g_desc[g_count++] = d;
        }
    }
    return g_count;
}

uint32_t sel_registry_init(const sel_host* host) {
    return sel_registry_init_machine(host, NULL);
}

const sel_registry_stats* sel_registry_get_stats(void) {
    return &g_stats;
}

/* True when the comma list `list` names `id`. */
static int list_has(const char* list, const char* id) {
    size_t n = strlen(id);
    while (list && *list) {
        const char* e = strchr(list, ',');
        size_t l = e ? (size_t)(e - list) : strlen(list);
        if (l == n && strncmp(list, id, n) == 0) {
            return 1;
        }
        list = e ? e + 1 : NULL;
    }
    return 0;
}

const sel_tag* sel_registry_format_owner(const char* format) {
    const sel_tag* best = NULL;
    intptr_t best_pri = 0;
    uint32_t i;
    for (i = 0; i < g_count; i++) {
        const sel_tag* hooks = (const sel_tag*)sel_tag_find(g_desc[i], SEL_PLUGIN_TAGS, 0);
        const char* formats = hooks ? (const char*)sel_tag_find(hooks, SEL_RP_FORMATS, 0) : NULL;
        intptr_t pri;
        if (!formats || !list_has(formats, format)) {
            continue;
        }
        pri = (intptr_t)sel_tag_find(hooks, SEL_RP_PRIORITY, 0);
        if (!best || pri > best_pri) { /* ties: first registered wins */
            best = g_desc[i];
            best_pri = pri;
        }
    }
    return best;
}

uint32_t sel_registry_count(void) {
    return g_count;
}

const sel_tag* sel_registry_get(uint32_t index) {
    return index < g_count ? g_desc[index] : NULL;
}

const sel_tag* sel_registry_find(uint32_t kind, const char* id) {
    uint32_t i;
    for (i = 0; i < g_count; i++) {
        const char* pid = (const char*)sel_tag_find(g_desc[i], SEL_PLUGIN_ID, 0);
        if (sel_tag_find(g_desc[i], SEL_PLUGIN_KIND, 0) == kind && pid && strcmp(pid, id) == 0) {
            return g_desc[i];
        }
    }
    return NULL;
}

const char* sel_registry_default_output(const sel_tag* machine) {
    const char* list = (const char*)sel_tag_find(machine, SEL_MACHINE_AUDIO, 0);
    while (list && *list) {
        const char* end = strchr(list, ',');
        size_t len = end ? (size_t)(end - list) : strlen(list);
        uint32_t i;
        for (i = 0; i < g_count; i++) {
            const char* pid = (const char*)sel_tag_find(g_desc[i], SEL_PLUGIN_ID, 0);
            if (sel_tag_find(g_desc[i], SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_OUTPUT && pid &&
                strlen(pid) == len && strncmp(pid, list, len) == 0) {
                return pid;
            }
        }
        list = end ? end + 1 : NULL;
    }
    return "null";
}

static void problem(const char* plugin, const char* what, uint32_t tag) {
    char msg[96];
    snprintf(msg, sizeof msg, "plugin %s: %s tag 0x%04lX", plugin, what, (unsigned long)tag);
    if (g_host) {
        g_host->log(g_host, SEL_LOG_ERROR, msg);
    }
}

static void lint_list(const char* plugin, const sel_tag* list, sel_lint_report* r) {
    const sel_tag* it = list;
    const sel_tag* t;
    while ((t = sel_tag_next(&it)) != NULL) {
        const sel_tag* it2 = list;
        const sel_tag* u;
        sel_tag_type type;
        if (t->tag & SEL_TAG_USER) {
            continue;
        }
        if (!sel_tag_type_of(t->tag, &type)) {
            r->unknown_tags++;
            problem(plugin, "unknown", t->tag);
        }
        while ((u = sel_tag_next(&it2)) != NULL && u != t) {
            if (u->tag == t->tag) {
                r->duplicate_tags++;
                problem(plugin, "duplicate", t->tag);
                break;
            }
        }
    }
}

void sel_registry_lint(sel_lint_report* r) {
    uint32_t i;
    memset(r, 0, sizeof *r);
    r->plugins = g_count;
    for (i = 0; i < g_count; i++) {
        const char* id = (const char*)sel_tag_find(g_desc[i], SEL_PLUGIN_ID, (uintptr_t) "?");
        const sel_tag* sub = (const sel_tag*)sel_tag_find(g_desc[i], SEL_PLUGIN_TAGS, 0);
        lint_list(id, g_desc[i], r);
        if (sub) {
            lint_list(id, sub, r);
        }
    }
}
