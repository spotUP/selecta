#include <selecta/machine.h>
#include <selecta/output.h>
#include <selecta/registry.h>

#include <selecta/engine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SEL_HAVE_LOADER
#include "loader.h"
#endif

/* Generated into the build tree by cmake/selecta_registry.cmake. */
extern const sel_plugin_entry_fn sel_registry_entries[];
extern const uint32_t sel_registry_entry_count;

/* Registry tables grow with the plugin count (I1133): a fixed bound dropped modules each time the build gained
 * plugins (I1130b: 64, I1133: 192 slots, with static and module copies of one plugin both counting). */
#define SEL_ORIGIN_MAX 512

static const sel_host* g_host;
static const sel_tag** g_desc;
static const char** g_origin;
static uint32_t g_count;
static uint32_t g_cap;
static sel_registry_stats g_stats;
static int g_has_fpu = 1; /* the init machine's, for sel_registry_format_owner */

void sel_registry_note_module_load(void) {
    g_stats.module_loads++;
}

void sel_registry_note_module_unload(void) {
    g_stats.module_unloads++;
}

/* Dynamic entries (plugin loader, I427a): added by a loader before init, walked after the static array. */
static struct dyn_entry {
    sel_plugin_entry_fn fn;
    char* origin;
    int builtin;
}* g_dyn;
static uint32_t g_dyn_cap;
static uint32_t g_dyn_count;
static uint32_t g_load_rejected; /* rejects a loader reported before init; they persist for the process */
#ifdef SEL_HAVE_LOADER
static int g_scanned;
#endif

#define SEL_BUILTIN_MISSING_MAX 64
static char g_missing[SEL_BUILTIN_MISSING_MAX][96];
static uint32_t g_missing_n;

static void reject_line(const char* path, const char* reason);

static int add_dyn(sel_plugin_entry_fn fn, const char* origin, int builtin) {
    const char* o = origin != NULL ? origin : "?";
    char* copy;
    if (fn == NULL) {
        return 0;
    }
    if (g_dyn_count >= g_dyn_cap) {
        uint32_t cap = g_dyn_cap != 0 ? g_dyn_cap * 2u : 64u;
        struct dyn_entry* grown = (struct dyn_entry*)realloc(g_dyn, cap * sizeof *g_dyn);
        if (grown == NULL) {
            return 0;
        }
        g_dyn = grown;
        g_dyn_cap = cap;
    }
    copy = (char*)malloc(strlen(o) + 1u);
    if (copy == NULL) {
        return 0;
    }
    memcpy(copy, o, strlen(o) + 1u);
    g_dyn[g_dyn_count].builtin = builtin;
    g_dyn[g_dyn_count].fn = fn;
    g_dyn[g_dyn_count].origin = copy;
    g_dyn_count++;
    return 1;
}

int sel_registry_add_entry(sel_plugin_entry_fn fn, const char* origin) {
    return add_dyn(fn, origin, 0);
}

int sel_registry_add_builtin_entry(sel_plugin_entry_fn fn, const char* origin) {
    return add_dyn(fn, origin, 1);
}

void sel_registry_note_builtin_missing(const char* file_name) {
    if (g_missing_n < SEL_BUILTIN_MISSING_MAX) {
        snprintf(g_missing[g_missing_n++], sizeof g_missing[0], "%s", file_name);
    }
    reject_line(file_name, "built-in module missing or not loadable");
}

const char* sel_registry_builtin_missing_name(uint32_t index) {
    return index < g_missing_n ? g_missing[index] : NULL;
}

static void reject_line(const char* path, const char* reason) {
    char msg[SEL_ORIGIN_MAX + 96];
    snprintf(msg, sizeof msg, "plugin module %s rejected: %s", path, reason);
    if (g_host != NULL) {
        g_host->log(g_host, SEL_LOG_WARN, msg);
    }
}

static char g_last_reject[SEL_ORIGIN_MAX + 96];

void sel_registry_note_rejected(const char* path, const char* reason) {
    g_load_rejected++;
    reject_line(path, reason);
    snprintf(g_last_reject, sizeof g_last_reject, "%s: %s", path, reason);
}

const char* sel_registry_last_reject(void) {
    return g_last_reject;
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

/* I1313c: the hidden list of the last init, one record per unmet need, and its display lines. */
static sel_registry_hidden* g_hidden;
static uint32_t g_hidden_n, g_hidden_cap;
static char* g_hidden_text;
static size_t g_hidden_text_used, g_hidden_text_cap;

const char* sel_registry_hidden_reason_name(uint32_t reason) {
    switch (reason) {
    case SEL_REGISTRY_HIDDEN_MIN_CPU:
        return "min_cpu";
    case SEL_REGISTRY_HIDDEN_NEEDS_FPU:
        return "needs_fpu";
    case SEL_REGISTRY_HIDDEN_MEMORY:
        return "memory";
    case SEL_REGISTRY_HIDDEN_CHIP:
        return "chip";
    default:
        return "?";
    }
}

/* KB rounded up, so a need never reads smaller than it is. */
static unsigned long kb_up(uint32_t bytes) {
    return ((unsigned long)bytes + 1023ul) / 1024ul;
}

static void hidden_line(char* out, size_t n, const char* id, uint32_t reason, uint32_t needed, uint32_t available) {
    switch (reason) {
    case SEL_REGISTRY_HIDDEN_MIN_CPU:
        snprintf(out, n, "%s off: needs a %lu processor, this one is a %lu", id, (unsigned long)needed,
                 (unsigned long)available);
        break;
    case SEL_REGISTRY_HIDDEN_NEEDS_FPU:
        snprintf(out, n, "%s off: needs a floating point unit, none found", id);
        break;
    case SEL_REGISTRY_HIDDEN_MEMORY:
        snprintf(out, n, "%s off: needs %lu KB of free memory, %lu KB free", id, kb_up(needed), available / 1024ul);
        break;
    default:
        snprintf(out, n, "%s off: needs %lu KB of chip memory, %lu KB found", id, kb_up(needed), available / 1024ul);
        break;
    }
}

/* Records one unmet need of `d`; a failed allocation drops the record, never the filter verdict. */
static void note_hidden(const sel_tag* d, uint32_t reason, uintptr_t needed, uintptr_t available) {
    const char* id = (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "?");
    char line[160];
    size_t len;
    if (g_hidden_n >= g_hidden_cap) {
        uint32_t cap = g_hidden_cap != 0 ? g_hidden_cap * 2u : 16u;
        sel_registry_hidden* grown = (sel_registry_hidden*)realloc(g_hidden, cap * sizeof *grown);
        if (grown == NULL) {
            return;
        }
        g_hidden = grown;
        g_hidden_cap = cap;
    }
    hidden_line(line, sizeof line, id, reason, (uint32_t)needed, (uint32_t)available);
    len = strlen(line) + 1u;
    if (g_hidden_text_used + len + 1u > g_hidden_text_cap) {
        size_t cap = g_hidden_text_cap != 0 ? g_hidden_text_cap * 2u : 1024u;
        char* grown;
        while (cap < g_hidden_text_used + len + 1u) {
            cap *= 2u;
        }
        grown = (char*)realloc(g_hidden_text, cap);
        if (grown == NULL) {
            return;
        }
        g_hidden_text = grown;
        g_hidden_text_cap = cap;
    }
    memcpy(g_hidden_text + g_hidden_text_used, line, len);
    g_hidden[g_hidden_n].id = id;
    g_hidden[g_hidden_n].reason = reason;
    g_hidden[g_hidden_n].needed = (uint32_t)needed;
    g_hidden[g_hidden_n].available = (uint32_t)available;
    g_hidden[g_hidden_n].line = NULL; /* set once the text buffer stops moving (sel_registry_hidden_get) */
    g_hidden_text_used += len;
    g_hidden_text[g_hidden_text_used] = '\0';
    g_hidden_n++;
}

uint32_t sel_registry_hidden_count(void) {
    return g_hidden_n;
}

const sel_registry_hidden* sel_registry_hidden_get(uint32_t index) {
    const char* p = g_hidden_text;
    uint32_t i;
    if (index >= g_hidden_n || p == NULL) {
        return NULL;
    }
    for (i = 0; i < index; i++) {
        p += strlen(p) + 1u;
    }
    g_hidden[index].line = p;
    return &g_hidden[index];
}

const char* sel_registry_hidden_text(void) {
    return g_hidden_text != NULL ? g_hidden_text : "";
}

/* The one machine filter (desktop and 68k). Returns 1 when `d` runs on `machine` and counts the
 * first unmet need per reason in g_stats; a plugin failing several needs counts under each and
 * leaves one hidden record per need (I1313c). */
static int fits(const sel_tag* d, const sel_tag* machine) {
    const sel_tag* hooks = (const sel_tag*)sel_tag_find(d, SEL_PLUGIN_TAGS, 0);
    uintptr_t min_cpu = hooks ? sel_tag_find(hooks, SEL_RP_MIN_CPU, 0) : 0;
    uintptr_t floats = hooks ? sel_tag_find(hooks, SEL_RP_NEEDS_FLOAT, 0) : 0;
    uintptr_t fpu_code = hooks ? sel_tag_find(hooks, SEL_RP_NEEDS_FPU, 0) : 0;
    uintptr_t memory = hooks ? sel_tag_find(hooks, SEL_RP_MEMORY, 0) : 0;
    uintptr_t chip = hooks ? sel_tag_find(hooks, SEL_OUT_MEM_CHIP, 0) : 0;
    int ok = 1;

    if (machine == NULL) {
        return 1;
    }
    if (min_cpu > cpu_number(sel_tag_find(machine, SEL_MACHINE_CPU, 0))) {
        g_stats.hidden_min_cpu++;
        note_hidden(d, SEL_REGISTRY_HIDDEN_MIN_CPU, min_cpu, cpu_number(sel_tag_find(machine, SEL_MACHINE_CPU, 0)));
        ok = 0;
    }
    if (floats && !sel_tag_find(machine, SEL_MACHINE_FPU, 0)) {
        g_stats.slow_needs_float++; /* soft-float: kept, ranked lower by sel_replayer_outranks (I1316) */
    }
    if (fpu_code && !sel_tag_find(machine, SEL_MACHINE_FPU, 0)) { /* I1316: 68881 code crashes without an FPU */
        g_stats.hidden_needs_fpu++;
        note_hidden(d, SEL_REGISTRY_HIDDEN_NEEDS_FPU, 1u, 0u);
        ok = 0;
    }
    /* I1313: SEL_RP_MEMORY is the player's own peak (module hunk plus allocations); it must fit in the RAM free
     * after Selecta has started, not the installed total. */
    if (memory > sel_tag_find(machine, SEL_MACHINE_FREE_RAM, 0)) {
        g_stats.hidden_memory++;
        note_hidden(d, SEL_REGISTRY_HIDDEN_MEMORY, memory, sel_tag_find(machine, SEL_MACHINE_FREE_RAM, 0));
        ok = 0;
    }
    if (chip > sel_tag_find(machine, SEL_MACHINE_CHIP_RAM, 0)) {
        g_stats.hidden_chip++;
        note_hidden(d, SEL_REGISTRY_HIDDEN_CHIP, chip, sel_tag_find(machine, SEL_MACHINE_CHIP_RAM, 0));
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
    } else {
        if (g_count >= g_cap) {
            uint32_t cap = g_cap != 0 ? g_cap * 2u : 128u;
            const sel_tag** nd = (const sel_tag**)realloc((void*)g_desc, cap * sizeof *nd);
            const char** no;
            if (nd != NULL) {
                g_desc = nd;
            }
            no = (const char**)realloc((void*)g_origin, cap * sizeof *no);
            if (no != NULL) {
                g_origin = no;
            }
            if (nd == NULL || no == NULL) {
                reject_line(origin, "out of memory, plugin dropped");
                return;
            }
            g_cap = cap;
        }
        g_origin[g_count] = origin;
        g_desc[g_count++] = d;
    }
}

uint32_t sel_registry_init_machine(const sel_host* host, const sel_tag* machine) {
    uint32_t i, j;
    const char** seen;
    uint32_t seen_n = 0;
    g_host = host;
    g_has_fpu = sel_machine_has_fpu(machine);
#ifdef SEL_HAVE_LOADER
    if (!g_scanned) {
        g_scanned = 1;
        sel_loader_scan();
    }
#endif
    seen = (const char**)malloc(((size_t)sel_registry_entry_count + g_dyn_count + 1u) * sizeof *seen);
    if (seen == NULL) {
        return 0;
    }
    g_count = 0;
    memset(&g_stats, 0, sizeof g_stats);
    g_hidden_n = 0;
    g_hidden_text_used = 0;
    if (g_hidden_text != NULL) {
        g_hidden_text[0] = '\0';
    }
    g_stats.dyn_rejected = g_load_rejected;
    g_stats.builtin_missing = g_missing_n;
    for (i = 0; i < sel_registry_entry_count; i++) {
        const sel_tag* d = sel_registry_entries[i](SEL_API_VERSION);
        if (!d) {
            continue;
        }
        seen[seen_n++] = (const char*)sel_tag_find(d, SEL_PLUGIN_ID, (uintptr_t) "");
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
        if (g_dyn[i].builtin) {
            g_stats.builtin_loaded++;
        } else {
            g_stats.dyn_loaded++;
        }
        keep(d, g_dyn[i].origin, machine);
    }
    free((void*)seen);
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
    const sel_tag* best_hooks = NULL;
    uint32_t i;
    for (i = 0; i < g_count; i++) {
        const sel_tag* hooks = (const sel_tag*)sel_tag_find(g_desc[i], SEL_PLUGIN_TAGS, 0);
        const char* formats = hooks ? (const char*)sel_tag_find(hooks, SEL_RP_FORMATS, 0) : NULL;
        if (!formats || !list_has(formats, format)) {
            continue;
        }
        if (sel_replayer_outranks(hooks, best_hooks, g_has_fpu)) {
            best = g_desc[i];
            best_hooks = hooks;
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

const char* sel_registry_default_ui(const sel_tag* machine) {
    (void)machine;
#ifdef __APPLE__
    /* I1023: a double-click on Selecta.app opens the desktop UI; a build without it falls back to text. */
    if (sel_registry_find(SEL_PLUGIN_KIND_UI, "desktop") != NULL) {
        return "desktop";
    }
#endif
#if defined(__amigaos__) || defined(AMIGA)
    /* I1325: a start with no console (Workbench icon, `run >NIL: <NIL:`) takes the MUI UI; the text UI needs a
     * console and gave only a "no terminal" requester. A MUI that cannot open falls back to text with its own
     * console (apps/selecta/main.c, MUI-P1-4). */
    if (sel_registry_find(SEL_PLUGIN_KIND_UI, "mui") != NULL) {
        return "mui";
    }
#endif
    return "text";
}

const char* sel_registry_default_backend(const sel_tag* machine) {
    return strcmp(sel_registry_default_ui(machine), "desktop") == 0 ? "sdl" : NULL;
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
        while (t->tag != SEL_RP_SYSFILE && (u = sel_tag_next(&it2)) != NULL && u != t) { /* SYSFILE repeats by design */
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

static void sysfile_warn(const char* plugin, const char* why) {
    char msg[160];
    snprintf(msg, sizeof msg, "plugin %s: %s", plugin, why);
    if (g_host != NULL) {
        g_host->log(g_host, SEL_LOG_WARN, msg);
    }
}

static void sysfile_warn_cb(void* ctx, const char* plugin, const char* why) {
    (void)ctx;
    sysfile_warn(plugin, why);
}

unsigned sel_registry_sysfiles(const sel_tag* hooks, sel_sysfile_decl* out, unsigned max) {
    if (hooks == NULL) {
        return 0;
    }
    return sel_sysfile_decls(hooks, sel_plugin_owner_id(sel_registry_count, sel_registry_get, hooks), out, max, sysfile_warn_cb, NULL);
}
