/* MUI Menustrip rows and menu dispatch from the shared action table (plan 2026-10-25-amiga-mui.md MUI-P2-3, P2-4).
 * The table is src/uimodel/actions.c, the one the desktop UI reads; nothing here keeps its own list of actions. */
#include <selecta/engine.h>
#include <selecta/uimodel.h>

#include <string.h>

#include "mui_map.h"

/* Copy the next path part (up to a '/' or the end) into out; returns the position after the separator. */
static const char* split_part(const char* p, char* out, size_t cap) {
    size_t n = 0;
    while (*p != '\0' && *p != '/') {
        if (n + 1u < cap) {
            out[n++] = *p;
        }
        p++;
    }
    out[n] = '\0';
    return *p == '/' ? p + 1 : p;
}

/* MUI-P3-8: the style a "VIEW_STYLE <name>" row selects, SEL_VIEW_STYLE_COUNT for any other row. */
static uint32_t row_style(const sel_uim_action* a) {
    uint32_t s;
    if (strncmp(a->line, "VIEW_STYLE ", 11) != 0) {
        return SEL_VIEW_STYLE_COUNT;
    }
    for (s = 0; s < SEL_VIEW_STYLE_COUNT; s++) {
        if (strcmp(a->line + 11, sel_view_style_label(s)) == 0) {
            return s;
        }
    }
    return SEL_VIEW_STYLE_COUNT;
}

uint32_t mui_menu_build(mui_menu_item* out, uint32_t cap) {
    return mui_menu_build_for(out, cap, SEL_VIEW_STYLES_ALL);
}

uint32_t mui_menu_build_for(mui_menu_item* out, uint32_t cap, uint32_t style_mask) {
    uint32_t i, n = sel_uim_action_count(), count = 0;
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        const char* p;
        mui_menu_item* m;
        if (a->menu == NULL || a->menu[0] == '\0') {
            continue;
        }
        if (row_style(a) < SEL_VIEW_STYLE_COUNT && (style_mask & SEL_VIEW_STYLE_BIT(row_style(a))) == 0u) {
            continue; /* a style this machine does not offer has no menu entry */
        }
        if (count < cap) {
            m = &out[count];
            p = split_part(a->menu, m->title, sizeof m->title);
            p = split_part(p, m->item, sizeof m->item);
            m->sub[0] = '\0';
            if (*p != '\0') { /* View/Visualizer/Levels: the middle part is the sub menu, the last the item */
                strcpy(m->sub, m->item);
                split_part(p, m->item, sizeof m->item);
            }
            m->key = a->key;
            m->shortcut = (a->key[0] != '\0' && a->key[1] == '\0') ? a->key[0] : '\0';
            m->id = a->id;
        }
        count++;
    }
    return count;
}

const char* mui_menu_line(const char* id) {
    uint32_t i, n = sel_uim_action_count();
    if (id == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        if (strcmp(a->id, id) == 0) {
            return a->line;
        }
    }
    return NULL;
}
