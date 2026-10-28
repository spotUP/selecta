/* Selecta_AAButton (I665 spike, aa_draw backends of I662). MUI 3.8: MUI_CreateCustomClass on Area, same dispatcher pattern
 * as mui_canvas_class.c. Draw: the super class fills the parent background (so the corners show the panel), then rows of
 * one flat colour with the insets of mui_aa_round_inset, then the caption:
 *   RTG (>= 15 bit screen): ttengine TT_Text with RGB foreground/background (truecolour blend inside ttengine);
 *   palette (AGA): TT_GetPixmap coverage mapped through a ObtainBestPen ramp between caption and face colour (ttengine
 *     does not smooth on palette RastPorts), drawn as runs of RectFill;
 *   no ttengine.library or no TTF: graphics Text() with the MUI font, same rounded shape.
 * No bevel pens, no shine, no shadow. Pressed state is the derived colour from mui_aa_pressed_rgb. */
#include "mui_aabtn_class.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <inline/macros.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_aa.h"

extern struct Library* MUIMasterBase;

/* ttengine.library 7.x, V6 interface (Libs_68k). Only the calls used here; tags and LVOs from its Developer/include. */
#define TTE_FontFile 0x6EDA0000UL
#define TTE_FontSize 0x6EDA0003UL
#define TTE_Screen 0x6EDA0006UL
#define TTE_FontAscender 0x6EDA0008UL
#define TTE_FontDescender 0x6EDA0009UL
#define TTE_Antialias 0x6EDA000FUL
#define TTE_Foreground 0x6EDA0018UL
#define TTE_Background 0x6EDA0019UL
struct TTE_Pixmap {
    ULONG size, width, height;
    UBYTE* data;
};
static struct Library* TTEngineBase;
static APTR tte_open_font(struct TagItem* tl) {
    return LP1(0x1E, APTR, TT_OpenFontA, struct TagItem*, tl, a0, , TTEngineBase);
}
static void tte_set_font(struct RastPort* rp, APTR font) {
    LP2NR(0x24, TT_SetFont, struct RastPort*, rp, a1, APTR, font, a0, , TTEngineBase);
}
static void tte_close_font(APTR font) {
    LP1NR(0x2A, TT_CloseFont, APTR, font, a0, , TTEngineBase);
}
static void tte_text(struct RastPort* rp, const char* str, ULONG n) {
    LP3NR(0x30, TT_Text, struct RastPort*, rp, a1, APTR, (APTR)str, a0, ULONG, n, d0, , TTEngineBase);
}
static void tte_set_attrs(struct RastPort* rp, struct TagItem* tl) {
    LP2NR(0x36, TT_SetAttrsA, struct RastPort*, rp, a1, struct TagItem*, tl, a0, , TTEngineBase);
}
static void tte_get_attrs(struct RastPort* rp, struct TagItem* tl) {
    LP2NR(0x3C, TT_GetAttrsA, struct RastPort*, rp, a1, struct TagItem*, tl, a0, , TTEngineBase);
}
static struct TTE_Pixmap* tte_pixmap(APTR font, const char* str, ULONG n) {
    struct TagItem none = {TAG_DONE, 0};
    return LP4(0x54, struct TTE_Pixmap*, TT_GetPixmapA, APTR, font, a1, APTR, (APTR)str, a2, ULONG, n, d0, struct TagItem*, &none, a0, , TTEngineBase);
}
static void tte_free_pixmap(struct TTE_Pixmap* pm) {
    LP1NR(0x5A, TT_FreePixmap, struct TTE_Pixmap*, pm, a0, , TTEngineBase);
}

#define RAMP 8
#define PAD_X 14
#define PAD_Y 6
#define RADIUS 6
#define TTE_PX 15

typedef struct aabtn_data {
    char caption[48], widest[48];
    uint32_t face, press, text;
    APTR font;
    struct TTE_Pixmap* pix;
    int rtg, backend, minw, minh;
    LONG face_pen, press_pen, text_pen, ramp[2][RAMP];
    struct ColorMap* cm;
} aabtn_data;

static struct MUI_CustomClass* g_class;
static const char* const g_ttf[] = {"RAM:Inter-Regular.ttf", "Selecta:Fonts/Inter-Regular.ttf", "FONTS:Inter-Regular.ttf", NULL};

static LONG pen_for(struct ColorMap* cm, uint32_t rgb) {
    struct TagItem tl[2] = {{OBP_Precision, PRECISION_GUI}, {TAG_DONE, 0}};
    return ObtainBestPenA(cm, ((rgb >> 16) & 0xFFu) * 0x01010101UL, ((rgb >> 8) & 0xFFu) * 0x01010101UL, (rgb & 0xFFu) * 0x01010101UL, tl);
}

static void rebuild_pixmap(aabtn_data* d, const char* s) {
    if (d->pix != NULL) {
        tte_free_pixmap(d->pix);
        d->pix = NULL;
    }
    if (d->font != NULL) {
        d->pix = tte_pixmap(d->font, s, (ULONG)strlen(s));
    }
}

static ULONG aabtn_setup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    struct Screen* scr;
    int i;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    scr = _screen(obj);
    d->cm = scr->ViewPort.ColorMap;
    d->rtg = GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) >= 15;
    d->press = mui_aa_pressed_rgb(d->face);
    if (TTEngineBase == NULL) {
        TTEngineBase = OpenLibrary((CONST_STRPTR) "ttengine.library", 6);
    }
    if (TTEngineBase != NULL && d->font == NULL) {
        for (i = 0; g_ttf[i] != NULL && d->font == NULL; i++) {
            struct TagItem tl[4] = {{TTE_FontFile, (ULONG)g_ttf[i]}, {TTE_FontSize, TTE_PX}, {TTE_Screen, (ULONG)scr}, {TAG_DONE, 0}};
            d->font = tte_open_font(tl);
        }
    }
    d->face_pen = pen_for(d->cm, d->face);
    d->press_pen = pen_for(d->cm, d->press);
    d->text_pen = pen_for(d->cm, d->text);
    if (!d->rtg && d->font != NULL) {
        for (i = 0; i < RAMP; i++) {
            d->ramp[0][i] = pen_for(d->cm, mui_aa_ramp_rgb(d->text, d->face, RAMP, i));
            d->ramp[1][i] = pen_for(d->cm, mui_aa_ramp_rgb(d->text, d->press, RAMP, i));
        }
    }
    if (d->font != NULL) {
        rebuild_pixmap(d, d->widest);
        d->minw = d->pix != NULL ? (int)d->pix->width : 0;
        d->minh = d->pix != NULL ? (int)d->pix->height : 0;
        rebuild_pixmap(d, d->caption);
    } else {
        struct RastPort tmp;
        InitRastPort(&tmp);
        SetFont(&tmp, _font(obj));
        d->minw = TextLength(&tmp, (CONST_STRPTR)d->widest, (ULONG)strlen(d->widest));
        d->minh = _font(obj)->tf_YSize;
    }
    return TRUE;
}

static ULONG aabtn_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    int i;
    if (d->cm != NULL) {
        if (d->face_pen >= 0) ReleasePen(d->cm, d->face_pen);
        if (d->press_pen >= 0) ReleasePen(d->cm, d->press_pen);
        if (d->text_pen >= 0) ReleasePen(d->cm, d->text_pen);
        for (i = 0; i < RAMP && !d->rtg && d->font != NULL; i++) {
            if (d->ramp[0][i] >= 0) ReleasePen(d->cm, d->ramp[0][i]);
            if (d->ramp[1][i] >= 0) ReleasePen(d->cm, d->ramp[1][i]);
        }
        d->cm = NULL;
    }
    if (d->pix != NULL) {
        tte_free_pixmap(d->pix);
        d->pix = NULL;
    }
    if (d->font != NULL) {
        tte_close_font(d->font);
        d->font = NULL;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aabtn_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    int w = d->minw + 2 * PAD_X, h = d->minh + 2 * PAD_Y;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aabtn_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aabtn_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG sel = 0;
    int l, t, w, h, y, s, len;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    GetAttr(MUIA_Selected, obj, &sel);
    s = sel ? 1 : 0;
    SetAPen(rp, s ? d->press_pen : d->face_pen);
    for (y = 0; y < h; y++) {
        int in = mui_aa_round_inset(w, h, RADIUS, y);
        RectFill(rp, l + in, t + y, l + w - 1 - in, t + y);
    }
    len = (int)strlen(d->caption);
    if (d->font != NULL && d->pix != NULL && d->rtg) {
        long asc = 0, desc = 0;
        struct TagItem fg[5] = {{TTE_Screen, (ULONG)_screen(obj)}, {TTE_Antialias, 2}, {TTE_Foreground, d->text}, {TTE_Background, s ? d->press : d->face}, {TAG_DONE, 0}};
        struct TagItem gt[3] = {{TTE_FontAscender, (ULONG)&asc}, {TTE_FontDescender, (ULONG)&desc}, {TAG_DONE, 0}};
        SetAPen(rp, d->text_pen);
        SetDrMd(rp, JAM1);
        tte_set_font(rp, d->font);
        tte_set_attrs(rp, fg);
        tte_get_attrs(rp, gt);
        if (desc < 0) desc = -desc;
        Move(rp, l + (w - (int)d->pix->width) / 2, t + (h - (int)(asc + desc)) / 2 + (int)asc);
        tte_text(rp, d->caption, (ULONG)len);
        d->backend = 2;
    } else if (d->font != NULL && d->pix != NULL) {
        int x0 = l + (w - (int)d->pix->width) / 2, y0 = t + (h - (int)d->pix->height) / 2;
        int px, py;
        for (py = 0; py < (int)d->pix->height; py++) {
            const UBYTE* row = d->pix->data + (size_t)py * d->pix->width;
            for (px = 0; px < (int)d->pix->width;) {
                int idx = mui_aa_ramp_index(row[px], RAMP), run = px + 1;
                while (run < (int)d->pix->width && mui_aa_ramp_index(row[run], RAMP) == idx) run++;
                if (idx != 0) {
                    SetAPen(rp, d->ramp[s][idx]);
                    RectFill(rp, x0 + px, y0 + py, x0 + run - 1, y0 + py);
                }
                px = run;
            }
        }
        d->backend = 3;
    } else {
        int tw;
        SetFont(rp, _font(obj));
        tw = TextLength(rp, (CONST_STRPTR)d->caption, (ULONG)len);
        SetAPen(rp, d->text_pen);
        SetBPen(rp, s ? d->press_pen : d->face_pen);
        SetDrMd(rp, JAM1);
        Move(rp, l + (w - tw) / 2, t + (h - _font(obj)->tf_YSize) / 2 + _font(obj)->tf_Baseline);
        Text(rp, (CONST_STRPTR)d->caption, (ULONG)len);
        d->backend = 1;
    }
    return 0;
}

static ULONG __attribute__((used)) aabtn_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aabtn_dispatch(void);
__asm__(".text\n.even\n.globl _aabtn_dispatch\n_aabtn_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aabtn_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aabtn_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case MUIM_Setup:
        return aabtn_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aabtn_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aabtn_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aabtn_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* mui_aabtn_new(const char* caption, const char* widest, uint32_t face, uint32_t text, LONG id) {
    Object* o;
    aabtn_data* d;
    if (g_class == NULL) {
        g_class = MUI_CreateCustomClass(NULL, (char*)MUIC_Area, NULL, sizeof(aabtn_data), (APTR)(uintptr_t)aabtn_dispatch);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_None, MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_Weight, 100,
                  MUIA_ShortHelp, (ULONG)caption, TAG_DONE);
    if (o != NULL) {
        d = INST_DATA(g_class->mcc_Class, o);
        strncpy(d->caption, caption, sizeof d->caption - 1);
        strncpy(d->widest, widest, sizeof d->widest - 1);
        d->face = face;
        d->text = text;
        d->face_pen = d->press_pen = d->text_pen = -1;
        memset(d->ramp, 0xFF, sizeof d->ramp);
        DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    }
    return o;
}

void mui_aabtn_set_caption(Object* obj, const char* caption) {
    aabtn_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (strcmp(d->caption, caption) == 0) {
        return;
    }
    strncpy(d->caption, caption, sizeof d->caption - 1);
    if (d->cm != NULL) {
        rebuild_pixmap(d, d->caption);
    }
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

int mui_aabtn_backend(Object* obj) {
    return ((aabtn_data*)INST_DATA(g_class->mcc_Class, obj))->backend;
}

void mui_aabtn_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
    if (TTEngineBase != NULL) {
        CloseLibrary(TTEngineBase);
        TTEngineBase = NULL;
    }
}
