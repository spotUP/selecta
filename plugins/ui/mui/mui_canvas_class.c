/* Selecta_Canvas (MUI-P5-1, MUI-P5-2). MUI 3.8 only (muimaster v19): MUI_CreateCustomClass on Area. Setup opens
 * Picasso96API.library for the pixel push (the Replay has no cybergraphics.library, same as the desktop RTG backend),
 * Show sizes the canvas to the object, Draw renders the grid with src/gui/draw and writes it to the window RastPort inside
 * the object rectangle, Hide and Cleanup release. Planar screens have no Picasso96: Draw then leaves the object blank
 * until MUI-P5-7 draws 1-bit covers there. */
#include "mui_canvas_class.h"

#include <selecta/platform.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <inline/macros.h>
#include "amiga/p96_write.h"
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>

extern struct Library* MUIMasterBase;
extern struct GfxBase* GfxBase; /* opened by mui_open, renamed by mui_gfxbase.h (I719) */

/* The instance data stays small: BOOPSI keeps the instance size in a UWORD (struct IClass cl_InstSize), and
 * sel_gui_canvas alone is about 165 KB (the INDEX8 cubes). Embedding it truncated the size MUI allocated, and the
 * OM_NEW memset then zeroed about 131 KB of other tasks' memory (the guest lost exec and the network seconds after
 * the MUI window opened). The canvas lives on the heap, owned from OM_NEW to OM_DISPOSE. */
typedef struct canvas_data {
    mui_canvas* mc;
    mui_grid_style style;
    int rtg;
    struct Library* p96;
    const mui_tile* tiles;
    uint32_t n, cursor;
    mui_art_fn art;
    void* art_ctx;
    uint32_t* img;   /* I1440q3: real cover words (img_edge squared), owned; NULL = the generated cover */
    int32_t img_edge;
    int single;      /* I1399d: one cover over the whole object (Now Playing art), not a grid */
    uint32_t kind, tint; /* tint: 0x00RRGGBB of the kind token, the planar fill */
    char title[64], ext[12];
} canvas_data;

/* Build-time guard: BOOPSI keeps the instance size in a UWORD (cl_InstSize). */
typedef char canvas_data_fits_uword[(sizeof(canvas_data) < 65536u) ? 1 : -1];

static struct MUI_CustomClass* g_class;

void mui_canvas_push(struct Library* p96, const uint32_t* px, int32_t w, int32_t h, struct RastPort* rp, int32_t dx, int32_t dy) {
    struct p96_render_info ri;
    ri.memory = (APTR)px;
    ri.pad = 0;
    ri.bytes_per_row = (WORD)(w * 4);
    ri.rgb_format = P96_FMT_A8R8G8B8;
    p96_write_pixel_array(p96, &ri, 0, 0, rp, dx, dy, w, h);
}

static ULONG canvas_new(struct IClass* cl, Object* obj, Msg msg) {
    obj = (Object*)DoSuperMethodA(cl, obj, msg);
    if (obj != NULL) {
        canvas_data* d = INST_DATA(cl, obj);
        memset(d, 0, sizeof *d);
        d->mc = calloc(1, sizeof *d->mc);
        if (d->mc == NULL) {
            CoerceMethod(cl, obj, OM_DISPOSE);
            return 0;
        }
    }
    return (ULONG)obj;
}

static ULONG canvas_dispose(struct IClass* cl, Object* obj, Msg msg) {
    canvas_data* d = INST_DATA(cl, obj);
    free(d->img);
    d->img = NULL;
    if (d->mc != NULL) {
        mui_canvas_release(d->mc);
        free(d->mc);
        d->mc = NULL;
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Kind label above the item title, bottom left (Johan 2026-11-15), drawn with graphics.library Text() in the window font: MUI has no
 * glyph atlas for src/gui, and the smaller change is two Text() calls. Pen SHINEPEN: white on the stock palettes, unverified on the rig. */
static void canvas_text(Object* obj, const char* s, LONG y) {
    struct RastPort* rp = _rp(obj);
    struct TextExtent te;
    LONG n = (LONG)strlen(s), fit;
    if (n == 0) {
        return;
    }
    fit = (LONG)TextFit(rp, (CONST_STRPTR)s, (ULONG)n, &te, NULL, 1, (LONG)_mwidth(obj) - 8, (LONG)rp->TxHeight + 2);
    Move(rp, _mleft(obj) + 4, y);
    Text(rp, (CONST_STRPTR)s, (ULONG)fit);
}

static void canvas_label(Object* obj, const canvas_data* d) {
    struct RastPort* rp = _rp(obj);
    LONG base = (LONG)rp->TxBaseline, h = (LONG)rp->TxHeight, bottom = _mtop(obj) + _mheight(obj) - 4;
    if (_mheight(obj) < 3 * h || _dri(obj) == NULL) { /* too small for text: the icon alone */
        return;
    }
    SetAPen(rp, _dri(obj)->dri_Pens[SHINEPEN]);
    SetDrMd(rp, JAM1);
    canvas_text(obj, d->title, bottom - h + base);
    canvas_text(obj, sel_cover_kind_label(d->kind), bottom - 2 * h + base);
}

static ULONG canvas_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    canvas_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) == 0) {
        return 0;
    }
    if (d->single && !d->rtg) { /* AGA path: the kind tint as the nearest palette pen, then the label and title */
        struct ColorMap* cm = _screen(obj)->ViewPort.ColorMap;
        LONG pen = ObtainBestPenA(cm, ((d->tint >> 16) & 0xFFu) * 0x01010101u, ((d->tint >> 8) & 0xFFu) * 0x01010101u, (d->tint & 0xFFu) * 0x01010101u, NULL);
        SetAPen(_rp(obj), (ULONG)pen);
        RectFill(_rp(obj), _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj));
        ReleasePen(cm, (ULONG)pen);
        canvas_label(obj, d);
        return 0;
    }
    if (d->p96 == NULL || !d->rtg) {
        return 0;
    }
    if (mui_canvas_fit(d->mc, (sel_gui_rect){_mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj)})) {
        if (d->single) {
            mui_canvas_draw_art(d->mc, (sel_gui_rect){0, 0, d->mc->c.w, d->mc->c.h}, d->img != NULL ? MUI_ART_DATATYPE : MUI_ART_COVER, d->img,
                                d->img_edge, d->img_edge, d->kind, d->ext, d->title);
            mui_canvas_push(d->p96, d->mc->pix, d->mc->c.w, d->mc->c.h, _rp(obj), _mleft(obj), _mtop(obj));
            canvas_label(obj, d);
            return 0;
        }
        mui_canvas_draw_grid(d->mc, &d->style, d->tiles, d->n, d->cursor, d->art, d->art_ctx);
        mui_canvas_push(d->p96, d->mc->pix, d->mc->c.w, d->mc->c.h, _rp(obj), _mleft(obj), _mtop(obj));
    }
    return 0;
}

/* Every Area subclass answers AskMinMax: the super call leaves only the frame, and the canvas adds a small minimum, a
 * grid-sized default and an unbounded maximum so the group can give it the space the list would have. */
static ULONG canvas_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    canvas_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (d->single) { /* the art frame is fixed: the cover asks for almost nothing and takes what it gets */
        msg->MinMaxInfo->MinWidth += 8;
        msg->MinMaxInfo->DefWidth += 8;
        msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
        msg->MinMaxInfo->MinHeight += 8;
        msg->MinMaxInfo->DefHeight += 8;
        msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
        return 0;
    }
    msg->MinMaxInfo->MinWidth += 16;
    msg->MinMaxInfo->DefWidth += 320;
    msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += 16;
    msg->MinMaxInfo->DefHeight += 200;
    msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
    return 0;
}

static ULONG __attribute__((used)) canvas_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

/* MUI calls dispatchers with cl=a0, obj=a2, msg=a1; this trampoline moves them to the stack for the C function. */
extern ULONG canvas_dispatch(void);
__asm__(".text\n.even\n.globl _canvas_dispatch\n_canvas_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _canvas_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) canvas_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    canvas_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return canvas_new(cl, obj, msg);
    case OM_DISPOSE:
        return canvas_dispose(cl, obj, msg);
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg)) {
            return FALSE;
        }
        d = INST_DATA(cl, obj);
        d->p96 = OpenLibrary((CONST_STRPTR) "Picasso96API.library", 0);
        return TRUE;
    case MUIM_Cleanup:
        d = INST_DATA(cl, obj);
        if (d->p96 != NULL) {
            CloseLibrary(d->p96);
            d->p96 = NULL;
        }
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show:
        d = INST_DATA(cl, obj);
        mui_canvas_fit(d->mc, (sel_gui_rect){_mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj)});
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Hide:
        d = INST_DATA(cl, obj);
        mui_canvas_release(d->mc);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_AskMinMax:
        return canvas_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return canvas_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

extern const sel_gui_icons_packed mui_cover_icons_baked; /* mui_cover_icons_baked.c, built by plugins/ui/mui/CMakeLists.txt */

Object* mui_canvas_new(const mui_grid_style* style, int screen_is_rtg) {
    Object* o;
    canvas_data* d;
    mui_canvas_set_icons(&mui_cover_icons_baked);
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(canvas_data), (APTR)(uintptr_t)canvas_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_ShowMe, FALSE, MUIA_FillArea, FALSE, MUIA_Weight, 100, TAG_DONE);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        d->style = *style;
        d->rtg = screen_is_rtg;
    }
    return o;
}

void mui_canvas_set_grid(Object* obj, const mui_tile* tiles, uint32_t n, uint32_t cursor, mui_art_fn art, void* art_ctx) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    d->tiles = tiles;
    d->n = n;
    d->cursor = cursor;
    d->art = art;
    d->art_ctx = art_ctx;
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

uint32_t mui_canvas_draws(Object* obj) {
    return ((canvas_data*)INST_DATA(g_class->mcc_Class, obj))->mc->draws;
}

void mui_canvas_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}

Object* mui_canvas_cover_new(int screen_is_rtg) {
    static const mui_grid_style none = {0u, 0u, 0u, 0, 0, 0};
    Object* o = mui_canvas_new(&none, screen_is_rtg);
    if (o != NULL) {
        ((canvas_data*)INST_DATA(g_class->mcc_Class, o))->single = 1;
        SetAttrs(o, MUIA_ShowMe, TRUE, TAG_DONE);
    }
    return o;
}

void mui_canvas_set_image(Object* obj, const uint32_t* px, int32_t edge) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    size_t n = (size_t)edge * (size_t)edge * sizeof *px;
    free(d->img);
    d->img = px != NULL ? malloc(n) : NULL;
    if (d->img != NULL) {
        memcpy(d->img, px, n);
    }
    d->img_edge = d->img != NULL ? edge : 0;
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

void mui_canvas_set_cover(Object* obj, uint32_t kind, uint32_t tint, const char* ext, const char* title) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    char e[sizeof d->ext], t[sizeof d->title];
    snprintf(e, sizeof e, "%s", ext != NULL ? ext : "");
    snprintf(t, sizeof t, "%s", title != NULL ? title : "");
    /* the Now Playing refresh runs on every pattern row: redraw only when the cover changed */
    if (d->kind == kind && d->tint == tint && strcmp(d->ext, e) == 0 && strcmp(d->title, t) == 0) {
        return;
    }
    d->kind = kind;
    d->tint = tint;
    memcpy(d->ext, e, sizeof e);
    memcpy(d->title, t, sizeof t);
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}
