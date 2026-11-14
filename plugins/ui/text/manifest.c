/* Text UI: the registry side on the Amiga (I1378b, thoughts/shared/research/2026-11-13-amiga-plugins-on-demand.md,
 * design 1). The selecta executables link only this unit; the UI itself (text_ui.c, the cell grid, the UI model) is the
 * LoadSeg module Players/sel_text_ui, loaded when --ui text (or the default UI pick) opens it. Only the tags the UI
 * picker reads before the load live here: the display name and the view styles. */
#include <selecta/engine.h>
#include <selecta/plugin.h>
#include <selecta/ui.h>

static sel_tag g_hooks[3];
static sel_tag g_desc[6];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "Text");
    set(&g_hooks[1], SEL_UI_VIEW_STYLES, SEL_VIEW_STYLES_ALL);
    set(&g_hooks[2], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "text");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_text_ui");
    set(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
