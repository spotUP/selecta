/* Selecta_AAList (I667i, design section 3.6). Same dispatcher pattern as mui_aacycle_class.c. Row maths, keys and colours come from
 * map/mui_aa.c (mui_aa_list_*); drawn with aa_round_fill / aa_text and held pens; no colour literal, no ObtainBestPen. The group built by
 * mui_aalist_new pairs the list with an AAScroller; the two are kept in step with MUIM_Notify one way and set with MUIA_NoNotify the other. */
#include "mui_aalist_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui_aa_draw.h"
#include "mui_aascroller_class.h"

#define AAL_TEXT_CAP 160
#define AAL_WHEEL_UP 0x7A
#define AAL_WHEEL_DOWN 0x7B
#define AAL_WHEEL_STEP 3

typedef struct aalist_data {
    long count, active, first, hover, vis, picked;
    mui_aalist_row_fn row;
    mui_aalist_cell_fn cell;
    mui_aalist_cols cols;
    long playing;
    int title, double_pick, no_select;
    ULONG last_s, last_m;
    long last_row;
    void* ctx;
    int max_rows;
    Object* scroller;
    mui_aa_list_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aalist_data;

static struct MUI_CustomClass* g_list_class;
static uint32_t g_list_made;

typedef struct aalist_fit {
    aalist_data* d;
    Object* self;
} aalist_fit;

/* Width callback of mui_aa_text_fit: the ink width of the first len bytes in the object's font. */
static int aalist_width(void* ctx, const char* s, int len) {
    aalist_fit* f = (aalist_fit*)ctx;
    char buf[AAL_TEXT_CAP];
    if (len >= AAL_TEXT_CAP) {
        len = AAL_TEXT_CAP - 1;
    }
    memcpy(buf, s, (size_t)len);
    buf[len] = '\0';
    return aa_text_width(&f->d->aa, f->self, buf);
}

static int aalist_title_h(const aalist_data* d) {
    return mui_aa_list_title_h(&d->m, d->title);
}

static int aalist_visible(Object* obj, const aalist_data* d) {
    return mui_aa_list_visible(&d->m, _mheight(obj) - aalist_title_h(d));
}

static const char* aalist_text(const aalist_data* d, long row, int col) {
    const char* s = d->cell != NULL ? d->cell(d->ctx, row, col) : (col == 0 && d->row != NULL ? d->row(d->ctx, row) : NULL);
    return s != NULL ? s : "";
}

/* Tell the scroller the window of rows (no notification back: the scroller only reports user drags). */
static void aalist_sync_scroller(Object* obj, aalist_data* d) {
    if (d->scroller != NULL && aa_live(obj)) {
        d->vis = aalist_visible(obj, d);
        SetAttrs(d->scroller, MUIA_NoNotify, TRUE, AAS_Visible, (ULONG)d->vis, AAS_Entries, (ULONG)d->count, AAS_First, (ULONG)d->first, TAG_DONE);
    }
}

static ULONG aalist_setup(struct IClass* cl, Object* obj, Msg msg) {
    aalist_data* d = INST_DATA(cl, obj);
    int st;
    uint32_t face, text;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_list_metrics(d->aa.font_px, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_SELECTED; st++) {
        if (st == MUI_AA_PRESSED || st == MUI_AA_DISABLED) {
            continue;
        }
        mui_aa_list_row_rgb(d->aa.tok, st, 0, &face, &text);
        aa_pen(&d->aa, face);
        aa_pen(&d->aa, text);
    }
    mui_aa_list_row_rgb(d->aa.tok, MUI_AA_NORMAL, 1, NULL, &text);
    aa_pen(&d->aa, text);
    mui_aa_list_header_rgb(d->aa.tok, &text, &face);
    aa_pen(&d->aa, text);
    aa_pen(&d->aa, face);
    d->hover = -1;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aalist_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aalist_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aalist_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aalist_data* d = INST_DATA(cl, obj);
    long rows = d->count < d->max_rows ? d->count : d->max_rows;
    int h = 2 * d->m.pad_y + aalist_title_h(d) + (int)(rows < 1 ? 1 : rows) * d->m.row_h;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 2 * d->m.pad_x + 40;
    msg->MinMaxInfo->DefWidth += 2 * d->m.pad_x + 120;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += 2 * d->m.pad_y + aalist_title_h(d) + d->m.row_h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

static ULONG aalist_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aalist_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    long k, vis, row;
    int l, t, w, h, th, c, n, th_h, y0;
    int cw[MUI_AALIST_COLS], fixed[MUI_AALIST_COLS];
    uint32_t face, text, muted, divider;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    vis = aalist_visible(obj, d);
    if (vis != d->vis) {
        d->first = mui_aa_list_scroll_to(d->first, vis, d->count, d->active);
        aalist_sync_scroller(obj, d);
    }
    mui_aa_list_row_rgb(d->aa.tok, MUI_AA_NORMAL, 0, &face, &text);
    mui_aa_list_header_rgb(d->aa.tok, &muted, &divider);
    aa_round_fill(rp, l, t, w, h, 0, aa_pen(&d->aa, face));
    th = aa_text_height(&d->aa, obj);
    n = d->cols.n < 1 ? 1 : d->cols.n;
    th_h = aalist_title_h(d);
    y0 = t + d->m.pad_y;
    for (c = 0; c < n; c++) {
        fixed[c] = d->cols.weight[c] > 0 || d->cols.sample[c] == NULL ? 0 : aa_text_width(&d->aa, obj, d->cols.sample[c]);
    }
    if (d->cols.n < 1) {
        cw[0] = w - 2 * d->m.pad_x;
    } else {
        mui_aa_list_col_widths(&d->m, w, n, d->cols.weight, fixed, cw);
    }
    if (d->title) {
        for (c = 0; c < n; c++) {
            aalist_fit fit;
            char buf[AAL_TEXT_CAP];
            fit.d = d;
            fit.self = obj;
            mui_aa_text_fit(aalist_text(d, -1, c), cw[c], aalist_width, &fit, buf, (int)sizeof buf);
            aa_text(&d->aa, obj, rp, l + mui_aa_list_col_x(&d->m, c, cw), y0 + (d->m.row_h - th) / 2, buf, muted, face);
        }
        aa_round_fill(rp, l, y0 + th_h - 1, w, 1, 0, aa_pen(&d->aa, divider));
    }
    for (c = 1; c < n; c++) {
        aa_round_fill(rp, l + mui_aa_list_col_x(&d->m, c, cw) - d->m.gap - 1, y0, 1, h - 2 * d->m.pad_y, 0, aa_pen(&d->aa, divider));
    }
    for (k = 0; k < vis && d->first + k < d->count; k++) {
        int y = y0 + th_h + (int)k * d->m.row_h;
        int st;
        row = d->first + k;
        st = row == d->active ? MUI_AA_SELECTED : row == d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
        mui_aa_list_row_rgb(d->aa.tok, st, row == d->playing, &face, &text);
        if (st != MUI_AA_NORMAL) {
            aa_round_fill(rp, l + 2, y, w - 4, d->m.row_h, d->m.radius, aa_pen(&d->aa, face));
            for (c = 1; c < n; c++) { /* the column dividers stay visible over the row highlight */
                aa_round_fill(rp, l + mui_aa_list_col_x(&d->m, c, cw) - d->m.gap - 1, y, 1, d->m.row_h, 0, aa_pen(&d->aa, divider));
            }
        }
        for (c = 0; c < n; c++) {
            aalist_fit fit;
            char buf[AAL_TEXT_CAP];
            uint32_t col = c == 0 ? text : muted;
            fit.d = d;
            fit.self = obj;
            mui_aa_text_fit(aalist_text(d, row, c), cw[c], aalist_width, &fit, buf, (int)sizeof buf);
            aa_text(&d->aa, obj, rp, l + mui_aa_list_col_x(&d->m, c, cw), y + (d->m.row_h - th) / 2, buf, col, face);
        }
    }
    return 0;
}

static void aalist_set_active(Object* obj, aalist_data* d, long a) {
    a = mui_aa_list_clamp_active(a, d->count);
    if (a != d->active) {
        long vis = aalist_visible(obj, d);
        set(obj, AAL_Active, (ULONG)a);
        set(obj, AAL_First, (ULONG)mui_aa_list_scroll_to(d->first, vis, d->count, a));
    }
}

static ULONG aalist_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aalist_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    int in, key = -1;
    long vis;
    ULONG act = 0;
    if (im == NULL) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    in = im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj);
    vis = aalist_visible(obj, d);
    if (im->Class == IDCMP_MOUSEMOVE) {
        long hv = in && !d->no_select ? mui_aa_list_row_at(&d->m, im->MouseY - _mtop(obj) - aalist_title_h(d), d->first, d->count) : -1;
        if (hv != d->hover) {
            d->hover = hv;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && in && !d->no_select) {
            long r = mui_aa_list_row_at(&d->m, im->MouseY - _mtop(obj) - aalist_title_h(d), d->first, d->count);
            if (r >= 0) {
                int dbl = r == d->last_row && DoubleClick(d->last_s, d->last_m, im->Seconds, im->Micros);
                aalist_set_active(obj, d, r);
                if (dbl || !d->double_pick) {
                    d->picked = r;
                    set(obj, AAL_Picked, (ULONG)r);
                }
                d->last_row = dbl ? -1 : r;
                d->last_s = im->Seconds;
                d->last_m = im->Micros;
                return MUI_EventHandlerRC_Eat;
            }
        }
        return 0;
    }
    if (im->Class != IDCMP_RAWKEY || (im->Code & IECODE_UP_PREFIX)) {
        return 0;
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &act);
    }
    if (im->Code == AAL_WHEEL_UP || im->Code == AAL_WHEEL_DOWN) {
        if (in) {
            long nf = d->first + (im->Code == AAL_WHEEL_UP ? -AAL_WHEEL_STEP : AAL_WHEEL_STEP);
            set(obj, AAL_First, (ULONG)mui_aa_list_clamp_first(nf, vis, d->count));
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (d->no_select || (!in && (Object*)act != obj)) {
        return 0;
    }
    switch (im->Code) {
    case 0x4C: key = MUI_AA_LIST_KEY_UP; break;
    case 0x4D: key = MUI_AA_LIST_KEY_DOWN; break;
    case 0x48: key = MUI_AA_LIST_KEY_PAGEUP; break;
    case 0x49: key = MUI_AA_LIST_KEY_PAGEDOWN; break;
    case 0x70: key = MUI_AA_LIST_KEY_HOME; break;
    case 0x71: key = MUI_AA_LIST_KEY_END; break;
    case 0x44:
    case 0x43:
        if (d->active >= 0) {
            d->picked = d->active;
            set(obj, AAL_Picked, (ULONG)d->active);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    default:
        return 0;
    }
    aalist_set_active(obj, d, mui_aa_list_move(d->active, d->count, vis, key));
    return MUI_EventHandlerRC_Eat;
}

static ULONG aalist_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aalist_data* d = INST_DATA(cl, obj);
    struct TagItem *tc = FindTagItem(AAL_Count, msg->ops_AttrList), *ta = FindTagItem(AAL_Active, msg->ops_AttrList),
                   *tf = FindTagItem(AAL_First, msg->ops_AttrList), *tcol = FindTagItem(AAL_Columns, msg->ops_AttrList),
                   *tp = FindTagItem(AAL_Playing, msg->ops_AttrList);
    int changed = 0;
    if (tcol != NULL && tcol->ti_Data != 0) {
        d->cols = *(const mui_aalist_cols*)tcol->ti_Data;
        changed = 1;
    }
    if (tp != NULL && (long)tp->ti_Data != d->playing) {
        d->playing = (long)tp->ti_Data;
        changed = 1;
    }
    if (tc != NULL) { /* the rows may have new text under the same count: always redraw */
        d->count = (long)tc->ti_Data;
        d->active = d->active < 0 ? -1 : mui_aa_list_clamp_active(d->active, d->count); /* none stays none */
        d->first = mui_aa_list_clamp_first(d->first, d->vis, d->count);
        changed = 1;
    }
    if (ta != NULL && (long)ta->ti_Data != d->active) {
        d->active = (long)ta->ti_Data < 0 ? -1 : mui_aa_list_clamp_active((long)ta->ti_Data, d->count);
        if (d->active >= 0) {
            d->first = mui_aa_list_scroll_to(d->first, d->vis, d->count, d->active); /* a program-set active row scrolls into view */
        }
        changed = 1;
    }
    if (d->no_select) {
        d->active = -1;
    }
    if (tf != NULL && (long)tf->ti_Data != d->first) {
        d->first = mui_aa_list_clamp_first((long)tf->ti_Data, d->vis, d->count);
        changed = 1;
    }
    if (changed && aa_live(obj)) {
        aalist_sync_scroller(obj, d);
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aalist_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aalist_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case AAL_Count: *msg->opg_Storage = (ULONG)d->count; return TRUE;
    case AAL_Active: *msg->opg_Storage = (ULONG)d->active; return TRUE;
    case AAL_First: *msg->opg_Storage = (ULONG)d->first; return TRUE;
    case AAL_Picked: *msg->opg_Storage = (ULONG)d->picked; return TRUE;
    case AAL_Playing: *msg->opg_Storage = (ULONG)d->playing; return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* AAM_GetRow: see mui_aalist_class.h. The geometry is the one aalist_draw uses (pad_y, title row, row_h from first). */
static ULONG aalist_getrow(struct IClass* cl, Object* obj, mui_aalist_getrow* g) {
    aalist_data* d = INST_DATA(cl, obj);
    int c, n = d->cols.n < 1 ? 1 : d->cols.n;
    long used = 0;
    if (g->row < 0 || g->row >= d->count || g->text == NULL || g->cap < 1) {
        return FALSE;
    }
    g->text[0] = '\0';
    for (c = 0; c < n; c++) {
        const char* s = aalist_text(d, g->row, c);
        long len = (long)strlen(s);
        if (len == 0) {
            continue;
        }
        if (used > 0 && used + 2 < g->cap) {
            g->text[used++] = ' ';
            g->text[used++] = ' ';
        }
        if (len > g->cap - 1 - used) {
            len = g->cap - 1 - used;
        }
        memcpy(g->text + used, s, (size_t)len);
        used += len;
        g->text[used] = '\0';
    }
    g->x = g->y = g->w = g->h = 0;
    g->selected = g->row == d->active;
    if (aa_live(obj) && g->row >= d->first && g->row < d->first + aalist_visible(obj, d)) {
        g->x = _mleft(obj);
        g->y = _mtop(obj) + d->m.pad_y + aalist_title_h(d) + (int)(g->row - d->first) * d->m.row_h;
        g->w = _mwidth(obj);
        g->h = d->m.row_h;
    }
    return TRUE;
}

static ULONG __attribute__((used)) aalist_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aalist_dispatch(void);
__asm__(".text\n.even\n.globl _aalist_dispatch\n_aalist_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aalist_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aalist_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_SET:
        return aalist_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aalist_get(cl, obj, (struct opGet*)msg);
    case AAM_GetRow:
        return aalist_getrow(cl, obj, (mui_aalist_getrow*)msg);
    case MUIM_Setup:
        return aalist_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aalist_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aalist_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aalist_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aalist_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aalist_new(const mui_aalist_spec* spec, Object** list_out) {
    Object *list, *scroller, *group;
    aalist_data* d;
    if (g_list_class == NULL) {
        g_list_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aalist_data), (APTR)(uintptr_t)aalist_dispatch);
    }
    if (g_list_class == NULL) {
        return NULL;
    }
    list = NewObject(g_list_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_CycleChain, (ULONG)TRUE, TAG_DONE);
    scroller = mui_aascroller_new();
    group = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 0, MUIA_Group_HorizSpacing, 0, Child, (ULONG)list, Child, (ULONG)scroller,
                          TAG_DONE);
    if (list == NULL || scroller == NULL || group == NULL) {
        if (group != NULL) {
            MUI_DisposeObject(group);
        } else {
            if (list != NULL) {
                MUI_DisposeObject(list);
            }
            if (scroller != NULL) {
                MUI_DisposeObject(scroller);
            }
        }
        return NULL;
    }
    d = INST_DATA(g_list_class->mcc_Class, list);
    d->count = spec->count;
    d->active = spec->no_select ? -1 : mui_aa_list_clamp_active(spec->active, spec->count);
    d->first = 0;
    d->hover = -1;
    d->row = spec->row;
    d->ctx = spec->ctx;
    d->max_rows = spec->max_rows > 0 ? spec->max_rows : 8;
    d->cell = spec->cell;
    if (spec->cols != NULL) {
        d->cols = *spec->cols;
    }
    d->playing = -1;
    d->last_row = -1;
    d->picked = -1;
    d->title = spec->title;
    d->double_pick = spec->double_pick;
    d->no_select = spec->no_select;
    d->scroller = scroller;
    DoMethod(scroller, MUIM_Notify, AAS_First, MUIV_EveryTime, list, 3, MUIM_Set, AAL_First, MUIV_TriggerValue);
    g_list_made++;
    if (list_out != NULL) {
        *list_out = list;
    }
    return group;
}

uint32_t mui_aalist_made(void) {
    return g_list_made;
}

void mui_aalist_class_free(void) {
    if (g_list_class != NULL) {
        MUI_DeleteCustomClass(g_list_class);
        g_list_class = NULL;
    }
    mui_aascroller_class_free();
}
