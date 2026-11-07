/* Spec to tag list (I685f, design 2026-11-06-mui-mcc-package.md section 8.3); see mui_aatagmap.h. */
#include "mui_aatagmap.h"
#if defined(__amigaos__) || defined(AMIGA)
#include <libraries/mui.h> /* the MUIA_* values the builders write (up.h has stand-ins only for the host) */
#endif

#include <stdint.h>
#include <string.h>

typedef struct tagw {
    struct TagItem* out;
    size_t cap, n;
} tagw;

static void tw_add(tagw* w, ULONG tag, UP_Data data) {
    if (w->n + 1 < w->cap) { /* keep the last slot for TAG_DONE */
        w->out[w->n].ti_Tag = tag;
        w->out[w->n].ti_Data = data;
    }
    w->n++;
}

static int tw_done(tagw* w) {
    if (w->n + 1 > w->cap) {
        return 0;
    }
    w->out[w->n].ti_Tag = TAG_DONE;
    w->out[w->n].ti_Data = 0;
    return (int)(w->n + 1);
}

#define TW_BEGIN(w, o, c) tagw w = {(o), (c), 0}

/* A function as a hook entry: the Amiga field is a function pointer, the host stand-in a void pointer. */
#define HOOK_FN(f) ((__typeof__(((struct Hook*)0)->h_SubEntry))(uintptr_t)(void (*)(void))(f))

int mui_aabtn_tags(const mui_aabtn_spec* sp, struct TagItem* out, size_t cap) {
    const char* help = sp->help != NULL ? sp->help : sp->caption;
    TW_BEGIN(w, out, cap);
    tw_add(&w, UPA_Button_Caption, UP_D(sp->caption));
    tw_add(&w, UPA_Button_Widest, UP_D(sp->widest));
    if (help != NULL) {
        tw_add(&w, MUIA_ShortHelp, UP_D(help));
    }
    tw_add(&w, UPA_Button_Icon, UP_D(sp->icon));
    tw_add(&w, UPA_Button_AltIcon, UP_D(sp->alt_icon));
    tw_add(&w, UPA_Button_IconOnly, UP_D(sp->icon_only));
    tw_add(&w, UPA_Button_Role, UP_D(sp->role));
    tw_add(&w, MUIA_Selected, UP_D(sp->selected));
    tw_add(&w, MUIA_Weight, UP_D(sp->weight));
    tw_add(&w, UPA_Button_FixedWidth, UP_D(sp->fixed_w));
    tw_add(&w, UPA_Button_FixedHeight, UP_D(sp->fixed_h));
    tw_add(&w, UPA_Button_Align, UP_D(sp->align));
    return tw_done(&w);
}

int mui_aatext_tags(const mui_aatext_spec* sp, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    tw_add(&w, UPA_Text_Contents, UP_D(sp->contents));
    tw_add(&w, UPA_Text_Widest, UP_D(sp->widest));
    tw_add(&w, UPA_Text_Tone, UP_D(sp->tone));
    tw_add(&w, UPA_Text_Align, UP_D(sp->align));
    tw_add(&w, UPA_Text_Ellipsis, UP_D(sp->ellipsis));
    tw_add(&w, UPA_Text_Fixed, UP_D(sp->fixed));
    tw_add(&w, MUIA_Frame, UP_D(sp->frame));
    if (sp->background != 0) {
        tw_add(&w, MUIA_Background, UP_D(sp->background));
    }
    tw_add(&w, MUIA_ShowMe, UP_D(!sp->hidden));
    return tw_done(&w);
}

int mui_aaslider_tags(const mui_aaslider_spec* sp, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    tw_add(&w, MUIA_Numeric_Min, UP_D(sp->lo));
    tw_add(&w, MUIA_Numeric_Max, UP_D(sp->hi));
    tw_add(&w, MUIA_Numeric_Value, UP_D(sp->value));
    if (sp->fix_w > 0) {
        tw_add(&w, MUIA_FixWidth, UP_D(sp->fix_w));
    }
    return tw_done(&w);
}

int mui_aacycle_tags(const mui_aacycle_spec* sp, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    tw_add(&w, UPA_Cycle_Entries, UP_D(sp->entries));
    tw_add(&w, UPA_Cycle_Active, UP_D(sp->active));
    return tw_done(&w);
}

int mui_aatabs_tags(const mui_aatabs_spec* sp, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    tw_add(&w, UPA_Tabs_Captions, UP_D(sp->captions));
    tw_add(&w, UPA_Tabs_Active, UP_D(sp->active));
    return tw_done(&w);
}

int mui_aafield_tags(const mui_aafield_spec* sp, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    tw_add(&w, UPA_Field_Tokens, UP_D(sp->tokens));
    tw_add(&w, MUIA_String_MaxLen, UP_D(sp->maxlen));
    tw_add(&w, UPA_Field_MaxWidth, UP_D(sp->max_w));
    tw_add(&w, UPA_Field_Placeholder, UP_D(sp->placeholder));
    return tw_done(&w);
}

int mui_aapanel_tags(const mui_aapanel_spec* sp, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    tw_add(&w, UPA_Panel_Tokens, UP_D(sp->tokens));
    tw_add(&w, UPA_Panel_Role, UP_D(sp->role));
    tw_add(&w, UPA_Panel_FixedWidth, UP_D(sp->fixed_px));
    tw_add(&w, UPA_Panel_FixedHeight, UP_D(sp->fixed_h));
    return tw_done(&w);
}

/* The hook bodies: the class passes a UP_ListCell, the owner's function takes row and column. The hook is the first member of its
 * holder in mui_aalist_hooks, which keeps the owner's function; h_Data is the spec's ctx. */
static UP_Data list_row_hook(struct Hook* h, void* obj, UP_ListCell* m) {
    const mui_aalist_row_hook* r = (const mui_aalist_row_hook*)h;
    (void)obj;
    return UP_D(r->fn(h->h_Data, m->row));
}

static UP_Data list_cell_hook(struct Hook* h, void* obj, UP_ListCell* m) {
    const mui_aalist_cell_hook* c = (const mui_aalist_cell_hook*)h;
    (void)obj;
    return UP_D(c->fn(h->h_Data, m->row, (int)m->col));
}

int mui_aalist_tags(const mui_aalist_spec* sp, mui_aalist_hooks* hooks, struct TagItem* out, size_t cap) {
    TW_BEGIN(w, out, cap);
    memset(hooks, 0, sizeof *hooks);
    tw_add(&w, UPA_List_Count, UP_D(sp->count));
    tw_add(&w, UPA_List_Active, UP_D(sp->active));
    if (sp->cell != NULL) {
        hooks->cell.fn = sp->cell;
        hooks->cell.hook.h_SubEntry = HOOK_FN(list_cell_hook);
        hooks->cell.hook.h_Data = sp->ctx;
        tw_add(&w, UPA_List_CellHook, UP_D(&hooks->cell.hook));
    } else if (sp->row != NULL) {
        hooks->row.fn = sp->row;
        hooks->row.hook.h_SubEntry = HOOK_FN(list_row_hook);
        hooks->row.hook.h_Data = sp->ctx;
        tw_add(&w, UPA_List_RowHook, UP_D(&hooks->row.hook));
    }
    if (sp->cols != NULL) {
        tw_add(&w, UPA_List_Columns, UP_D(sp->cols));
    }
    tw_add(&w, UPA_List_MaxRows, UP_D(sp->max_rows));
    tw_add(&w, UPA_List_Title, UP_D(sp->title));
    tw_add(&w, UPA_List_DoublePick, UP_D(sp->double_pick));
    tw_add(&w, UPA_List_NoSelect, UP_D(sp->no_select));
    return tw_done(&w);
}
