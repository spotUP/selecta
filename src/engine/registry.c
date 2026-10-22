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

uint32_t sel_registry_init(const sel_host* host) {
    uint32_t i;
    g_host = host;
    g_count = 0;
    for (i = 0; i < sel_registry_entry_count && g_count < SEL_REGISTRY_MAX; i++) {
        const sel_tag* d = sel_registry_entries[i](SEL_API_VERSION);
        if (d) {
            g_desc[g_count++] = d;
        }
    }
    return g_count;
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
