#include <selecta/machine.h>
#include <selecta/output.h>
#include <selecta/registry.h>

#include <selecta/engine.h>

#include <stdio.h>
#include <string.h>

#ifdef SEL_HAVE_LOADER
#include "loader.h"
#endif

/* Generated into the build tree by cmake/selecta_registry.cmake. */
extern const sel_plugin_entry_fn sel_registry_entries[];
extern const uint32_t sel_registry_entry_count;

#define SEL_REGISTRY_MAX 64

static const sel_host* g_host;
static const sel_tag* g_desc[SEL_REGISTRY_MAX];
static uint32_t g_count;
static sel_registry_stats g_stats;

/* Dynamic entries (plugin loader, I427a): added by a loader before init, walked after the static array. */
#define SEL_DYN_MAX 32
#define SEL_ORIGIN_MAX 512
static struct {
    sel_plugin_entry_fn fn;
    char origin[SEL_ORIGIN_MAX];
} g_dyn[SEL_DYN_MAX];
static uint32_t g_dyn_count;
static uint32_t g_load_rejected; /* rejects a loader reported before init; they persist for the process */
static const char* g_origin[SEL_REGISTRY_MAX];
#ifdef SEL_HAVE_LOADER
static int g_scanned;
#endif

int sel_registry_add_entry(sel_plugin_entry_fn fn, const char* origin) {
    if (fn == NULL || g_dyn_count >= SEL_DYN_MAX) {
        return 0;
    }
    g_dyn[g_dyn_count].fn = fn;
    snprintf(g_dyn[g_dyn_count].origin, sizeof g_dyn[g_dyn_count].origin, "%s", origin != NULL ? origin : "?");
    g_dyn_count++;
    return 1;
}

static void reject_line(const char* path, const char* reason) {
    char msg[SEL_ORIGIN_MAX + 96];
    snprintf(msg, sizeof msg, "plugin module %s rejected: %s", path, reason);
    if (g_host != NULL) {
        g_host->log(g_host, SEL_LOG_WARN, msg);
    }
}

void sel_registry_note_rejected(const char* path, const char* reason) {
    g_load_rejected++;
    reject_line(path, reason);
}

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

/* A descriptor a host can use: a known kind and a plugin id. */
static int descriptor_ok(const sel_tag* d) {
    uintptr_t kind = sel_tag_find(d, SEL_PLUGIN_KIND, 0);
    const char* id = (const char*)sel_tag_find(d, SEL_PLUGIN_ID, 0);
    return kind >= SEL_PLUGIN_KIND_REPLAYER && kind <= SEL_PLUGIN_KIND_SCRIPTING && id != NULL && id[0] != '\0';
}

static void keep(const sel_tag* d, const char* origin, const sel_tag* machine) {
    g_stats.total++;
    if (!fits(d, machine)) {
        g_stats.filtered++;
    } else if (g_count < SEL_REGISTRY_MAX) {
        g_origin[g_count] = origin;
        g_desc[g_count++] = d;
    }
}

uint32_t sel_registry_init_machine(const sel_host* host, const sel_tag* machine) {
    uint32_t i, j;
    const char* seen[SEL_REGISTRY_MAX + SEL_DYN_MAX];
    uint32_t seen_n = 0;
    g_host = host;
#ifdef SEL_HAVE_LOADER
    if (!g_scanned) {
        g_scanned = 1;
        sel_loader_scan();
    }
#endif
    g_count = 0;
    memset(&g_stats, 0, sizeof g_stats);
    g_stats.dyn_rejected = g_load_rejected;
    for (i = 0; i < sel_registry_entry_count; i++) {
        const sel_tag* d = sel_registry_entries[i](SEL_API_VERSION);
        if (!d) {
            continue;
        }
        if (seen_n < SEL_REGISTRY_MAX) {
            seen[seen_n++] = (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "");
        }
        keep(d, "static", machine);
    }
    for (i = 0; i < g_dyn_count; i++) {
        const sel_tag* d = g_dyn[i].fn(SEL_API_VERSION);
        const char* id;
        int dup = 0;
        if (d == NULL) {
            g_stats.dyn_rejected++;
            reject_line(g_dyn[i].origin, "entry returned NULL for this API version");
            continue;
        }
        if (!descriptor_ok(d)) {
            g_stats.dyn_rejected++;
            reject_line(g_dyn[i].origin, "malformed descriptor (kind or id missing)");
            continue;
        }
        id = (const char*)sel_tag_find(d, SEL_PLUGIN_ID, 0);
        for (j = 0; j < seen_n; j++) {
            if (strcmp(seen[j], id) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            char why[SEL_ORIGIN_MAX];
            snprintf(why, sizeof why, "duplicate plugin id %.64s (first registration wins)", id);
            g_stats.dyn_rejected++;
            reject_line(g_dyn[i].origin, why);
            continue;
        }
        seen[seen_n++] = id;
        g_stats.dyn_loaded++;
        keep(d, g_dyn[i].origin, machine);
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

const char* sel_registry_origin(uint32_t index) {
    return index < g_count ? g_origin[index] : NULL;
}

uint32_t sel_registry_count(void) {
    return g_count;
}

const sel_tag* sel_registry_get(uint32_t index) {
    return index < g_count ? g_desc[index] : NULL;
}

int sel_registry_ui_listed(const sel_tag* desc) {
    const sel_tag* hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    return hooks == NULL || sel_tag_find(hooks, SEL_UI_PRESENT, 1) != 0;
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

/* First entry of the comma list `list` that is a registered plugin of `kind`; NULL when none. */
static const char* first_registered(uint32_t kind, const char* list) {
    while (list && *list) {
        const char* end = strchr(list, ',');
        size_t len = end ? (size_t)(end - list) : strlen(list);
        uint32_t i;
        for (i = 0; i < g_count; i++) {
            const char* pid = (const char*)sel_tag_find(g_desc[i], SEL_PLUGIN_ID, 0);
            if (sel_tag_find(g_desc[i], SEL_PLUGIN_KIND, 0) == kind && pid && strlen(pid) == len &&
                strncmp(pid, list, len) == 0) {
                return pid;
            }
        }
        list = end ? end + 1 : NULL;
    }
    return NULL;
}

const char* sel_registry_default_output(const sel_tag* machine) {
    const char* list = (const char*)sel_tag_find(machine, SEL_MACHINE_AUDIO, 0);
    const char* pid = first_registered(SEL_PLUGIN_KIND_OUTPUT, list);
    return pid ? pid : "null";
}

uint32_t sel_registry_output_rate(const sel_tag* hooks, const sel_tag* machine) {
    sel_out_rate_for_fn fn = hooks ? (sel_out_rate_for_fn)sel_tag_find(hooks, SEL_OUT_RATE_FOR, 0) : NULL;
    uint32_t rate = fn ? fn(machine) : 0u;
    return rate != 0u ? rate : SEL_DEFAULT_OUTPUT_RATE;
}

const char* sel_registry_default_integration(const char* preferred) {
    const char* pid = first_registered(SEL_PLUGIN_KIND_INTEGRATION, preferred);
    return pid ? pid : "none";
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
