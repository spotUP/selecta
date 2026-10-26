/* Amiga MUI UI plugin: descriptor and hooks (plan 2026-10-25-amiga-mui.md MUI-P1-2). Same shape as the text and
 * desktop UI plugins. The MUI calls (MUI-P1-3) compile only on the Amiga with the MUI 3.8 headers; on the host this
 * file builds for the descriptor test and open always refuses. */
#include <selecta/engine.h>
#include <selecta/plugin.h>
#include <selecta/ui.h>

#include <stdint.h>
#include <string.h>

#include "map/mui_map.h"

typedef struct mui_state {
    uint32_t opens; /* ui_open_mui sentinel (MUI-P1-5): windows opened */
    sel_tag stats[2];
} mui_state;

static mui_state g_state;
static sel_tag g_hooks[8];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

static void* mui_open(const sel_ui_open_args* a) {
    (void)a;
    /* MUI-P1-3: muimaster.library v19, Application and one Window go here; the sentinel counts after the window
     * opens. Until then (and on the host) the plugin refuses, so the shell keeps its current UI. */
    return NULL;
}

static int mui_wait(void* state, uint32_t timeout_ms) {
    (void)state;
    (void)timeout_ms;
    return -1;
}

static void mui_close(void* state) {
    (void)state;
}

static const sel_tag* mui_stats(void* state) {
    mui_state* s = &g_state; /* one instance per process */
    (void)state;
    s->stats[0].tag = SEL_UI_STAT_OPEN_MUI;
    s->stats[0].value = s->opens;
    s->stats[1].tag = SEL_TAG_END;
    s->stats[1].value = 0;
    return s->stats;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    memset(&g_state, 0, sizeof g_state);
    set(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "MUI");
    set(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(mui_open));
    set(&g_hooks[2], SEL_UI_WAIT, SEL_TAG_FN(mui_wait));
    set(&g_hooks[3], SEL_UI_CLOSE, SEL_TAG_FN(mui_close));
    set(&g_hooks[4], SEL_UI_STATS, SEL_TAG_FN(mui_stats));
    /* Planar-safe mask: the descriptor is static, an RTG screen widens it when the window opens (MUI-P6). */
    set(&g_hooks[5], SEL_UI_VIEW_STYLES, mui_style_mask(0));
    set(&g_hooks[6], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "mui");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
