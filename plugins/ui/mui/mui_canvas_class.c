/* Selecta_Canvas (MUI-P5-1, MUI-P5-2). MUI 3.8 only (muimaster v19): MUI_CreateCustomClass on Area. Setup opens
 * Picasso96API.library for the pixel push (the Replay has no cybergraphics.library, same as the desktop RTG backend),
 * Show sizes the canvas to the object, Draw renders the grid with src/gui/draw and writes it to the window RastPort inside
 * the object rectangle, Hide and Cleanup release. Planar screens have no Picasso96: Draw then leaves the object blank
 * until MUI-P5-7 draws 1-bit covers there. */
#include "mui_canvas_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <inline/macros.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

extern struct Library* MUIMasterBase;

typedef struct canvas_data {
    mui_canvas mc;
    mui_grid_style style;
    int rtg;
    struct Library* p96;
    const mui_tile* tiles;
    uint32_t n, cursor;
    mui_art_fn art;
    void* art_ctx;
} canvas_data;

static struct MUI_CustomClass* g_class;

struct canvas_render_info {
    APTR memory;
    WORD bytes_per_row;
    WORD pad;
    ULONG rgb_format;
};
#define CANVAS_FMT_A8R8G8B8 6u /* Picasso96.h RGBFB_A8R8G8B8, as in backend_rtg.c */

static void canvas_push(struct Library* p96, const uint32_t* px, int32_t w, int32_t h, struct RastPort* rp, int32_t dx, int32_t dy) {
    struct canvas_render_info ri;
    ri.memory = (APTR)px;
    ri.pad = 0;
    ri.bytes_per_row = (WORD)(w * 4);
    ri.rgb_format = CANVAS_FMT_A8R8G8B8;
    LP8NR(0x66, p96WritePixelArray, struct canvas_render_info*, &ri, a0, UWORD, 0, d0, UWORD, 0, d1, struct RastPort*, rp, a1,
          UWORD, dx, d2, UWORD, dy, d3, UWORD, w, d4, UWORD, h, d5, , p96);
}

static ULONG canvas_new(struct IClass* cl, Object* obj, Msg msg) {
    obj = (Object*)DoSuperMethodA(cl, obj, msg);
    if (obj != NULL) {
        canvas_data* d = INST_DATA(cl, obj);
        memset(d, 0, sizeof *d);
    }
    return (ULONG)obj;
}

static ULONG canvas_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    canvas_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) == 0 || d->p96 == NULL || !d->rtg) {
        return 0;
    }
    if (mui_canvas_fit(&d->mc, (sel_gui_rect){_mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj)})) {
        mui_canvas_draw_grid(&d->mc, &d->style, d->tiles, d->n, d->cursor, d->art, d->art_ctx);
        canvas_push(d->p96, d->mc.pix, d->mc.c.w, d->mc.c.h, _rp(obj), _mleft(obj), _mtop(obj));
    }
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
        mui_canvas_fit(&d->mc, (sel_gui_rect){_mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj)});
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Hide:
        d = INST_DATA(cl, obj);
        mui_canvas_release(&d->mc);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Draw:
        return canvas_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_canvas_new(const mui_grid_style* style, int screen_is_rtg) {
    Object* o;
    canvas_data* d;
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
    return ((canvas_data*)INST_DATA(g_class->mcc_Class, obj))->mc.draws;
}

void mui_canvas_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
