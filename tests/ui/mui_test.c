/* mui-descriptor (MUI-P1-2) and mui-stat-tag (MUI-P0-4), plan 2026-10-25-amiga-mui.md.
 * The mui plugin is not registered on the host (MUI-P1-1), so the test calls its entry point directly; the text UI
 * comes from the registry. Usage: mui_test <scratch dir> */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/registry.h>
#include <selecta/ui.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

extern const sel_tag* sel_plugin_entry_mui_test(uint32_t host_api);

static int failures;

#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static int list_has(const sel_tag* t, uint32_t tag) {
    for (; t != NULL && t->tag != SEL_TAG_END; t++) {
        if (t->tag == tag) {
            return 1;
        }
    }
    return 0;
}

static int probe_yes(void) {
    return 1;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const sel_tag* desc = sel_plugin_entry_mui_test(1u);
    const sel_tag* hooks;
    sel_ui_stats_fn stats;
    const sel_tag* tdesc;
    const sel_tag* thooks;
    const sel_tag* st;
    sel_engine_config cfg;
    sel_engine* e;
    sel_ui_open_args a;
    const char* targv[4];
    char keys[1024];
    FILE* f;
    void* ui;
    uint32_t last = 0;

    if (argc < 2) {
        fputs("usage: mui_test <scratch dir>\n", stderr);
        return 2;
    }
    mui_set_probe(probe_yes); /* the host has no MUI; the entry lists the plugin only when the probe says so */
    desc = sel_plugin_entry_mui_test(1u);

    /* MUI-P1-2: id, kind, and the style mask per tier. */
    CHECK(desc != NULL, "no descriptor");
    hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    CHECK(desc != NULL && sel_tag_find(desc, SEL_PLUGIN_KIND, 0) == SEL_PLUGIN_KIND_UI, "kind is not UI");
    CHECK(desc != NULL && strcmp((const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0), "mui") == 0, "id is not mui");
    CHECK(mui_style_mask(1) == SEL_VIEW_STYLES_ALL, "RTG mask %x", (unsigned)mui_style_mask(1));
    CHECK(mui_style_mask(0) == SEL_VIEW_STYLES_CLASSIC, "planar mask %x", (unsigned)mui_style_mask(0));
    CHECK(mui_style_mask(1) != mui_style_mask(0), "tiers offer the same styles");
    CHECK(hooks != NULL && sel_tag_find(hooks, SEL_UI_VIEW_STYLES, 0) == mui_style_mask(0),
          "descriptor mask is not the planar-safe mask");
    CHECK(hooks != NULL && sel_tag_find(hooks, SEL_UI_OPEN, 0) != 0 && sel_tag_find(hooks, SEL_UI_WAIT, 0) != 0 &&
              sel_tag_find(hooks, SEL_UI_CLOSE, 0) != 0,
          "open/wait/close hooks missing");

    /* MUI-P0-4: the tag is the highest UI tag in tags.def, the mui plugin lists it, the text UI does not. */
#define SEL_TAG(grp, name, val, type) \
    if (strcmp(#grp, "UI") == 0 && (val) > last) last = (val);
#include <selecta/tags.def>
#undef SEL_TAG
    CHECK(SEL_UI_STAT_OPEN_MUI == last, "SEL_UI_STAT_OPEN_MUI %x is not the last UI tag %x", (unsigned)SEL_UI_STAT_OPEN_MUI,
          (unsigned)last);
    CHECK(SEL_UI_STAT_OPEN_MUI > SEL_UI_STAT_TOOLTIP_TARGETS, "tag not after the previous last tag");
    stats = hooks != NULL ? (sel_ui_stats_fn)sel_tag_find(hooks, SEL_UI_STATS, 0) : NULL;
    CHECK(stats != NULL, "mui plugin has no SEL_UI_STATS hook");
    st = stats != NULL ? stats(NULL) : NULL; /* one instance: the plugin keeps its counters in a static */
    CHECK(list_has(st, SEL_UI_STAT_OPEN_MUI), "mui stats lack ui_open_mui");
    CHECK(sel_tag_find(st, SEL_UI_STAT_OPEN_MUI, 99) == 0, "ui_open_mui is not 0 before any window opened");

    snprintf(keys, sizeof keys, "%s/mui_stat.keys", argv[1]);
    f = fopen(keys, "wb");
    if (f == NULL) {
        return 2;
    }
    fputs("q\n", f);
    fclose(f);
    sel_registry_init(host);
    tdesc = sel_registry_find(SEL_PLUGIN_KIND_UI, "text");
    thooks = tdesc != NULL ? (const sel_tag*)sel_tag_find(tdesc, SEL_PLUGIN_TAGS, 0) : NULL;
    CHECK(thooks != NULL, "text UI not registered");
    CHECK(sel_registry_find(SEL_PLUGIN_KIND_UI, "mui") != NULL, "mui is not registered on the host (MUI-P1-4)");
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    if (e != NULL && thooks != NULL) {
        targv[0] = "mui_test";
        targv[1] = "--keys";
        targv[2] = keys;
        targv[3] = NULL;
        memset(&a, 0, sizeof a);
        a.engine = e;
        a.host = host;
        a.argc = 3;
        a.argv = targv;
        ui = ((sel_ui_open_fn)sel_tag_find(thooks, SEL_UI_OPEN, 0))(&a);
        CHECK(ui != NULL, "text UI open failed");
        if (ui != NULL) {
            st = ((sel_ui_stats_fn)sel_tag_find(thooks, SEL_UI_STATS, 0))(ui);
            CHECK(st != NULL && !list_has(st, SEL_UI_STAT_OPEN_MUI), "text UI stats list ui_open_mui");
            CHECK(sel_tag_find(st, SEL_UI_STAT_OPEN_MUI, 0) == 0, "text UI ui_open_mui is not 0");
            ((sel_ui_close_fn)sel_tag_find(thooks, SEL_UI_CLOSE, 0))(ui);
        }
    }
    if (failures == 0) {
        puts("mui_test: ok");
    }
    return failures != 0;
}
