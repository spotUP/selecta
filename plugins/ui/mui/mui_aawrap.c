/* Selecta's wrappers around the UP- classes (I685f, design 2026-11-06-mui-mcc-package.md section 8.3). Each mui_<class>_new(spec)
 * builds the tag list from the spec (map/mui_aatagmap.c, the function the host test checks), calls the class constructor and adds
 * the MUIM_Application_ReturnID notify for the spec's id. The id never reaches a class. Amiga only. */
#include <stdlib.h>

#include <clib/alib_protos.h>
#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mui_aatagmap.h"
#include "mui_aawrap.h"
#include "mui_text.h"

static void notify_id(Object* o, ULONG attr, ULONG trigger, LONG id) {
    DoMethod(o, MUIM_Notify, attr, trigger, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
}

/* The hook storage of every list built: owned here, freed with the application (a list outlives no longer than that). */
typedef struct hook_node {
    struct hook_node* next;
    mui_aalist_hooks hooks;
} hook_node;
static hook_node* g_hooks;

void mui_aawrap_free(void) {
    mui_text_free();
    while (g_hooks != NULL) {
        hook_node* n = g_hooks;
        g_hooks = n->next;
        free(n);
    }
}

static Object* button_new(const mui_aabtn_spec* usp, ULONG key) {
    char c0[MUI_L1_TEXT], c1[MUI_L1_TEXT], c2[MUI_L1_TEXT];
    mui_aabtn_spec lsp;
    const mui_aabtn_spec* sp = &lsp;
    struct TagItem tl[MUI_AATAGMAP_MAX];
    struct TagItem more[3];
    Object* o;
    mui_aabtn_spec_l1(usp, &lsp, c0, c1, c2); /* I1139: Latin-1 for the class */
    if (mui_aabtn_tags(sp, tl, MUI_AATAGMAP_MAX) == 0) {
        return NULL;
    }
    more[0].ti_Tag = MUIA_ControlChar;
    more[0].ti_Data = key;
    more[1].ti_Tag = TAG_MORE;
    more[1].ti_Data = (ULONG)(uintptr_t)tl;
    more[2].ti_Tag = TAG_DONE;
    more[2].ti_Data = 0;
    o = UP_NewButton(key != 0 ? more : tl);
    if (o != NULL) {
        notify_id(o, sp->role == UPV_Button_Role_Toggle ? MUIA_Selected : MUIA_Pressed, sp->role == UPV_Button_Role_Toggle ? MUIV_EveryTime : FALSE, sp->id);
    }
    return o;
}

Object* mui_aabtn_new(const mui_aabtn_spec* sp) {
    return button_new(sp, 0);
}

Object* mui_aabtn_new_key(const mui_aabtn_spec* sp, ULONG key) {
    return button_new(sp, key);
}

Object* mui_aatext_new(const mui_aatext_spec* usp) {
    char c0[MUI_L1_TEXT], c1[MUI_L1_TEXT];
    mui_aatext_spec lsp;
    struct TagItem tl[MUI_AATAGMAP_MAX];
    mui_aatext_spec_l1(usp, &lsp, c0, c1); /* I1139 */
    return mui_aatext_tags(&lsp, tl, MUI_AATAGMAP_MAX) != 0 ? UP_NewText(tl) : NULL;
}

Object* mui_aaslider_new(const mui_aaslider_spec* sp) {
    struct TagItem tl[MUI_AATAGMAP_MAX];
    Object* o = mui_aaslider_tags(sp, tl, MUI_AATAGMAP_MAX) != 0 ? UP_NewSlider(tl) : NULL;
    if (o != NULL && sp->id != 0) {
        notify_id(o, MUIA_Numeric_Value, MUIV_EveryTime, sp->id);
    }
    return o;
}

Object* mui_aaprogress_new(long value, long max) {
    struct TagItem tl[3];
    tl[0].ti_Tag = UPA_Progress_Value;
    tl[0].ti_Data = (ULONG)value;
    tl[1].ti_Tag = UPA_Progress_Max;
    tl[1].ti_Data = (ULONG)max;
    tl[2].ti_Tag = TAG_DONE;
    tl[2].ti_Data = 0;
    return UP_NewProgress(tl);
}

Object* mui_aacycle_new(const mui_aacycle_spec* sp) {
    struct TagItem tl[MUI_AATAGMAP_MAX];
    Object* o = mui_aacycle_tags(sp, tl, MUI_AATAGMAP_MAX) != 0 ? UP_NewCycle(tl) : NULL;
    if (o != NULL && sp->id != 0) {
        notify_id(o, UPA_Cycle_Active, MUIV_EveryTime, sp->id);
    }
    return o;
}

Object* mui_aatabs_new(const mui_aatabs_spec* sp) {
    struct TagItem tl[MUI_AATAGMAP_MAX];
    return mui_aatabs_tags(sp, tl, MUI_AATAGMAP_MAX) != 0 ? UP_NewTabs(tl) : NULL;
}

Object* mui_aafield_new(const mui_aafield_spec* usp) {
    mui_aafield_spec lsp;
    const mui_aafield_spec* sp = &lsp;
    struct TagItem tl[MUI_AATAGMAP_MAX];
    mui_aafield_spec_l1(usp, &lsp); /* I1139 */
    Object* o = mui_aafield_tags(sp, tl, MUI_AATAGMAP_MAX) != 0 ? UP_NewField(tl) : NULL;
    if (o != NULL && sp->id != 0) {
        ULONG str = 0;
        GetAttr(UPA_Field_String, o, &str);
        if (str != 0) {
            notify_id((Object*)str, MUIA_String_Acknowledge, MUIV_EveryTime, sp->id);
        }
    }
    return o;
}

Object* mui_aapanel_new(const mui_aapanel_spec* sp) {
    struct TagItem tl[MUI_AATAGMAP_MAX];
    return mui_aapanel_tags(sp, tl, MUI_AATAGMAP_MAX) != 0 ? UP_NewPanel(tl) : NULL;
}

Object* mui_aalist_new(const mui_aalist_spec* sp, Object** list_out) {
    struct TagItem tl[MUI_AATAGMAP_MAX];
    hook_node* n = (hook_node*)calloc(1, sizeof *n);
    Object* g;
    if (n == NULL || mui_aalist_tags(sp, &n->hooks, tl, MUI_AATAGMAP_MAX) == 0) {
        free(n);
        return NULL;
    }
    n->hooks.row.hook.h_Entry = (__typeof__(n->hooks.row.hook.h_Entry))(void (*)(void))HookEntry;
    n->hooks.cell.hook.h_Entry = (__typeof__(n->hooks.cell.hook.h_Entry))(void (*)(void))HookEntry;
    n->next = g_hooks;
    g_hooks = n;
    g = UP_NewList(tl, list_out);
    return g;
}
