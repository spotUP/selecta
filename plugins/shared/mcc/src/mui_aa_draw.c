/* mui_aa_draw: see mui_aa_draw.h. The ttengine V6 calls come from the I665 spike (tags and LVOs from its Developer/include). */
#include "mui_aa_draw.h"
#include "map/mui_log.h"
#include "mui_aatip.h"
#include "map/mui_fontprobe.h"

#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <graphics/text.h>
#include <inline/macros.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
/* I685w run 6: diskfont.library for the PLAIN tone fonts; own base name, opened on first use (same reason as mui_gfxbase.h). */
#define DiskfontBase up_aa_DiskfontBase
#include <proto/diskfont.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include <selecta/uimodel/theme.h>
#include "map/mui_theme.h"
#include "map/mui_aa_paint.h"
#include "mui_aa_rtg.h"
#include "mcc/up.h"

#define TTE_FontFile 0x6EDA0000UL
#define TTE_FontSize 0x6EDA0003UL
#define TTE_Screen 0x6EDA0006UL
#define TTE_FontAscender 0x6EDA0008UL
#define TTE_Antialias 0x6EDA000FUL
#define TTE_Foreground 0x6EDA0018UL
#define TTE_Background 0x6EDA0019UL
struct TTE_Pixmap {
    ULONG size, width, height;
    UBYTE* data;
};
static struct Library* TTEngineBase;
/* I685w: the RTG fill library of the truecolour path, opened once per process in aa_setup, closed in aa_free. */
struct Library* DiskfontBase; /* up_aa_DiskfontBase: proto/diskfont.h declares it extern */
static struct Library* g_p96;
static struct Library* g_cgx;
static int g_rtg_tried;
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

/* One process wide font per px size, reference counted; closed when the last user cleans up. */
#define FONT_SLOTS 4
static struct {
    int px, refs;
    APTR font;
} g_fonts[FONT_SLOTS];
static int g_missing_logged;

/* I625a3: the probe names may sit on an assign or volume that is missing (a host assign); dos.library would open a "Please insert
 * volume" requester. The probe loop runs with the process's pr_WindowPtr = -1 and restores it. Same pattern as I214
 * (src/core/amiga/dos_quiet.c); the package cannot include src/core, so it carries its own copy of the two small calls. */
struct font_probe {
    struct Screen* scr;
    int px;
};
static void* probe_quiet_begin(void* ctx) {
    struct Process* me = (struct Process*)FindTask(NULL);
    void* saved;
    (void)ctx;
    if (me == NULL || me->pr_Task.tc_Node.ln_Type != NT_PROCESS) {
        return (void*)-2L; /* not a DOS process: nothing to change, nothing to restore */
    }
    saved = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1L;
    return saved;
}
static void probe_quiet_end(void* ctx, void* saved) {
    struct Process* me = (struct Process*)FindTask(NULL);
    (void)ctx;
    if (saved == (void*)-2L || me == NULL) {
        return;
    }
    me->pr_WindowPtr = saved;
}
static void* probe_open(void* ctx, const char* name) {
    const struct font_probe* fp = (const struct font_probe*)ctx;
    struct TagItem tl[4] = {{TTE_FontFile, (ULONG)name}, {TTE_FontSize, (ULONG)fp->px}, {TTE_Screen, (ULONG)fp->scr}, {TAG_DONE, 0}};
    return tte_open_font(tl);
}

static APTR font_get(struct Screen* scr, int px) {
    int i, free_slot = -1;
    for (i = 0; i < FONT_SLOTS; i++) {
        if (g_fonts[i].font != NULL && g_fonts[i].px == px) {
            g_fonts[i].refs++;
            return g_fonts[i].font;
        }
        if (g_fonts[i].font == NULL && free_slot < 0) {
            free_slot = i;
        }
    }
    if (TTEngineBase == NULL) {
        aa_trace("mcc: ttengine.library open begin");
        TTEngineBase = OpenLibrary((CONST_STRPTR) "ttengine.library", 6);
        aa_trace(TTEngineBase != NULL ? "mcc: ttengine.library open done (found)" : "mcc: ttengine.library open done (missing)");
    }
    if (TTEngineBase != NULL && free_slot >= 0) {
        struct font_probe fp;
        char tl_[64];
        static unsigned long n_probe;
        mui_fontprobe_ops ops;
        APTR f;
        fp.scr = scr;
        fp.px = px;
        ops.ctx = &fp;
        ops.quiet_begin = probe_quiet_begin;
        ops.quiet_end = probe_quiet_end;
        ops.open = probe_open;
        n_probe++;
        /* Every probe is one DOS Open per name; tracing each one cost about 8 ms a line on the rig, so only the first probe and every
         * 32nd are traced (the count in the line carries the rest). */
        if (n_probe == 1 || n_probe % 32 == 0) {
            snprintf(tl_, sizeof tl_, "mcc: Inter font probe %lu begin (%d px)", n_probe, px);
            aa_trace(tl_);
        }
        f = mui_fontprobe_open(&ops, mui_font_files());
        if (n_probe == 1 || n_probe % 32 == 0) {
            snprintf(tl_, sizeof tl_, "mcc: Inter font probe %lu done (%d px, %s)", n_probe, px, f != NULL ? "opened" : "not found");
            aa_trace(tl_);
        }
        if (f != NULL) {
            g_fonts[free_slot].px = px;
            g_fonts[free_slot].refs = 1;
            g_fonts[free_slot].font = f;
            return f;
        }
    }
    if (!g_missing_logged) {
        g_missing_logged = 1;
        aa_log("up-mcc: ttengine or Inter missing, plain text\n");
    }
    return NULL;
}

static void font_put(APTR font) {
    int i;
    for (i = 0; i < FONT_SLOTS; i++) {
        if (g_fonts[i].font == font && --g_fonts[i].refs <= 0) {
            tte_close_font(font);
            g_fonts[i].font = NULL;
        }
    }
}

static LONG obtain(struct ColorMap* cm, uint32_t rgb) {
    int prec[2], n = mui_theme_pen_precisions(prec, 2), i;
    LONG pen = -1;
    for (i = 0; i < n && pen < 0; i++) {
        struct TagItem tl[3] = {{OBP_Precision, (ULONG)(LONG)prec[i]}, {OBP_FailIfBad, (ULONG)(i + 1 < n)}, {TAG_DONE, 0}};
        pen = ObtainBestPenA(cm, ((rgb >> 16) & 0xFFu) * 0x01010101UL, ((rgb >> 8) & 0xFFu) * 0x01010101UL, (rgb & 0xFFu) * 0x01010101UL, tl);
    }
    return pen;
}

static LONG aa_pen(aa_ctx* c, uint32_t rgb) {
    int i;
    for (i = 0; i < c->npens; i++) {
        if (c->pen_rgb[i] == rgb) {
            return c->pen[i];
        }
    }
    if (c->npens >= AA_MAX_PENS) {
        return -1;
    }
    c->pen_rgb[c->npens] = rgb;
    c->pen[c->npens] = obtain(c->cm, rgb);
    return c->pen[c->npens++];
}

static aa_ctx* g_ctx_list;
#ifdef UP_CLASS_LIBRARY
#include <clib/alib_protos.h>
#include "map/up_store.h"
static ULONG retheme_hook_fn(struct Hook* hook, APTR app, ULONG* msg);
static struct Hook g_retheme_hook;
static void retheme_hook_init(void) {
    if (g_retheme_hook.h_Entry == NULL) {
        g_retheme_hook.h_Entry = (__typeof__(g_retheme_hook.h_Entry))(void (*)(void))HookEntry;
        g_retheme_hook.h_SubEntry = (__typeof__(g_retheme_hook.h_SubEntry))(void (*)(void))retheme_hook_fn;
    }
}
#endif

static void ctx_unlink(aa_ctx* c) {
    aa_ctx** p;
    for (p = &g_ctx_list; *p != NULL; p = &(*p)->next) {
        if (*p == c) {
            *p = c->next;
            break;
        }
    }
    c->next = NULL;
    c->linked = 0;
#ifdef UP_CLASS_LIBRARY
    (void)up_store_detach(c);
#endif
}

/* I685w: the colour aa_erase fills: the role colour of the nearest UP-Panel above the object (design 5.2 item 2), else BACKGROUND. */
static uint32_t ctx_bg(const aa_ctx* c) {
    ULONG p = 0, role = 0;
    Object* o = c->obj;
    int depth;
    for (depth = 0; depth < 12 && o != NULL; depth++) {
        p = 0;
        GetAttr(MUIA_Parent, o, &p);
        o = (Object*)p;
        if (o != NULL && GetAttr(UPA_Panel_Role, o, &role)) {
            return mui_aa_panel_rgb(c->tok, (int)role);
        }
    }
    return c->tok[SEL_GUI_TOKEN_BACKGROUND];
}

void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n) {
    int i, depth;
    if (c->linked) {
        ctx_unlink(c);
    }
    memset(c, 0, sizeof *c);
    c->obj = obj;
    c->next = g_ctx_list;
    g_ctx_list = c;
    c->linked = 1;
    memcpy(c->tok, mui_theme_tokens(), sizeof c->tok);
#ifdef UP_CLASS_LIBRARY
    /* I685n: a recolour started in another process (the prefs Save) reaches this object through the registry of up-mcc.library. */
    retheme_hook_init();
    (void)up_store_attach(c, obj, (void*)_app(obj), &g_retheme_hook);
#endif
    c->scr = _screen(obj);
    c->cm = c->scr->ViewPort.ColorMap;
    depth = (int)GetBitMapAttr(c->scr->RastPort.BitMap, BMA_DEPTH);
    /* I685e1: the screen's pixel shape from its DrawInfo (valid MUIM_Setup..MUIM_Cleanup, as this context); no display database call per object. */
    c->aspect_q8 = _dri(obj) != NULL ? mui_pixel_aspect_q8((int)_dri(obj)->dri_Resolution.X, (int)_dri(obj)->dri_Resolution.Y) : 256;
    if (!g_rtg_tried) {
        g_rtg_tried = 1;
        g_p96 = OpenLibrary((CONST_STRPTR) "Picasso96API.library", 0);
        if (g_p96 == NULL) {
            g_cgx = OpenLibrary((CONST_STRPTR) "cybergraphics.library", 41);
        }
    }
    c->tc = mui_aa_truecolour(depth, g_p96 != NULL, g_cgx != NULL);
    c->bg_rgb = ctx_bg(c);
    {
        static int logged;
        if (!logged && (c->tc || depth >= 15)) {
            logged = 1;
            aa_log(c->tc ? (g_p96 != NULL ? "up-mcc: truecolour via Picasso96API\n" : "up-mcc: truecolour via cybergraphics\n")
                         : "up-mcc: no RTG library, pens on a truecolour screen\n");
        }
    }
    c->font_px = mui_aa_font_px(c->scr->Height, c->scr->Width);
    c->font = c->font_px > 0 ? font_get(c->scr, c->font_px) : NULL;
    if (c->font == NULL) {
        c->backend = AA_PLAIN;
    } else if (depth >= 15) {
        c->backend = AA_TC;
        c->kind = MUI_AA_RAMP_TC;
    } else {
        c->backend = AA_PAL;
        c->kind = depth <= 4 ? MUI_AA_RAMP_AGA : MUI_AA_RAMP_RTG;
    }
    for (i = 0; i < n && !c->tc; i++) {
        aa_pen(c, rgb[i]);
    }
}

void aa_set_font_px(aa_ctx* c, int px) {
    APTR f;
    if (c->font == NULL || px == c->font_px || px <= 0) {
        return;
    }
    f = font_get(c->scr, px);
    if (f != NULL) {
        font_put(c->font);
        c->font = f;
        c->font_px = px;
    }
}

void aa_plain_font(aa_ctx* c, Object* obj, int px, int bold) {
    mui_aa_screen_font_t sf;
    struct TextAttr ta;
    struct TextFont* f;
    if (c->backend != AA_PLAIN || px <= 0) {
        return;
    }
    c->plain_bold = bold;
    if (!mui_aa_screen_font(px, &sf) || sf.ysize <= (int)_font(obj)->tf_YSize) {
        return; /* no larger stock size than the MUI font: keep it */
    }
    if (DiskfontBase == NULL) {
        DiskfontBase = OpenLibrary((CONST_STRPTR) "diskfont.library", 36);
    }
    if (DiskfontBase == NULL) {
        return;
    }
    ta.ta_Name = (STRPTR)sf.name;
    ta.ta_YSize = (UWORD)sf.ysize;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;
    f = OpenDiskFont(&ta);
    if (f != NULL) {
        if (c->plain != NULL) {
            CloseFont(c->plain);
        }
        c->plain = f;
        aa_log("up-mcc: plain tone font opened\n");
    }
}

struct TextFont* aa_plain_textfont(const aa_ctx* c, Object* obj) {
    return c->plain != NULL ? c->plain : _font(obj);
}

int aa_live(Object* obj) {
    return obj != NULL && muiRenderInfo(obj) != NULL && _win(obj) != NULL;
}

void aa_cleanup(aa_ctx* c) {
    int i;
    if (c->linked) {
        ctx_unlink(c);
    }
    if (c->cm != NULL) {
        for (i = 0; i < c->npens; i++) {
            if (c->pen[i] >= 0) {
                ReleasePen(c->cm, c->pen[i]);
            }
        }
    }
    c->npens = 0;
    c->cm = NULL;
    if (c->plain != NULL) {
        CloseFont(c->plain);
        c->plain = NULL;
    }
    if (c->font != NULL) {
        font_put(c->font);
        c->font = NULL;
    }
}

/* The stock String gadget draws through pens even on a truecolour screen, so its background needs a real pen. */
LONG aa_nearest_pen(aa_ctx* c, uint32_t rgb) {
    return aa_pen(c, rgb);
}

void aa_hold(aa_ctx* c, uint32_t rgb) {
    if (!c->tc) {
        (void)aa_pen(c, rgb);
    }
}

/* I685w: the three thin bindings of the mui_aa_paint_ops seam. The shapes themselves live in map/mui_aa_paint.c (host tested). */
struct paint_bind {
    aa_ctx* c;
    struct RastPort* rp;
};

static void bind_rect_rgb(void* ctx, int x0, int y0, int x1, int y1, uint32_t rgb) {
    struct paint_bind* b = (struct paint_bind*)ctx;
    if (g_p96 != NULL) {
        aa_p96_rect_fill(g_p96, b->rp, x0, y0, x1, y1, rgb & 0xFFFFFFu);
    } else {
        aa_cgx_fill_pixel_array(g_cgx, b->rp, x0, y0, x1 - x0 + 1, y1 - y0 + 1, rgb & 0xFFFFFFu);
    }
}

static void bind_rect_pen(void* ctx, int x0, int y0, int x1, int y1, long pen) {
    struct paint_bind* b = (struct paint_bind*)ctx;
    SetAPen(b->rp, (ULONG)pen);
    RectFill(b->rp, x0, y0, x1, y1);
}

static long bind_pen(void* ctx, uint32_t rgb) {
    return (long)aa_pen(((struct paint_bind*)ctx)->c, rgb);
}

static void bind_init(mui_aa_paint_ops* ops, struct paint_bind* b, aa_ctx* c, struct RastPort* rp) {
    b->c = c;
    b->rp = rp;
    ops->ctx = b;
    ops->tc = c->tc;
    ops->rect_rgb = bind_rect_rgb;
    ops->rect_pen = bind_rect_pen;
    ops->pen = bind_pen;
}

void aa_rect(aa_ctx* c, struct RastPort* rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_rect(&ops, x0, y0, x1, y1, rgb);
}

void aa_round_fill(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_round_fill(&ops, x, y, w, h, radius, rgb);
}

void aa_round_fill_hole(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, int hx, int hy, int hw, int hh) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_round_fill_hole(&ops, x, y, w, h, radius, rgb, hx, hy, hw, hh);
}

void aa_round_fill_holes(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, const int* holes, int n) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_round_fill_holes(&ops, x, y, w, h, radius, rgb, holes, n);
}

void aa_round_corners(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_round_corners(&ops, x, y, w, h, radius, rgb);
}

void aa_round_frame(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_round_frame(&ops, x, y, w, h, radius, rgb);
}

void aa_runs(aa_ctx* c, struct RastPort* rp, int x, int y, const unsigned char* slot, int n, const uint32_t* slot_rgb) {
    mui_aa_paint_ops ops;
    struct paint_bind b;
    bind_init(&ops, &b, c, rp);
    mui_aa_paint_runs(&ops, x, y, slot, n, slot_rgb);
}

void aa_erase(aa_ctx* c, Object* obj, int x, int y, int w, int h) {
    if (c->tc) {
        struct RastPort* rp = _rp(obj);
        if (rp != NULL) {
            aa_rect(c, rp, x, y, x + w - 1, y + h - 1, c->bg_rgb);
        }
        return;
    }
    DoMethod(obj, MUIM_DrawBackground, x, y, w, h, x, y, 0);
}

/* I685w: plain (no ttengine) text on a truecolour screen. Text() draws into a one plane scratch bitmap; each run of set bits goes out
 * as one aa_rect in fg. The glyphs are the screen font, one bit coverage, exact token colour. The bitmap lives for this call only. */
static void plain_text_tc(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg) {
    struct TextFont* f = aa_plain_textfont(c, obj);
    struct RastPort tmp;
    struct BitMap* bm;
    int len = (int)strlen(str), tw, th, row;
    InitRastPort(&tmp);
    SetFont(&tmp, f);
    if (c->plain_bold) {
        SetSoftStyle(&tmp, FSF_BOLD, AskSoftStyle(&tmp));
    }
    tw = (int)TextLength(&tmp, (CONST_STRPTR)str, (ULONG)len) + (c->plain_bold ? 1 : 0);
    th = (int)f->tf_YSize;
    if (tw <= 0 || th <= 0) {
        return;
    }
    bm = AllocBitMap((ULONG)tw, (ULONG)th, 1, BMF_CLEAR, NULL);
    if (bm == NULL) {
        return;
    }
    tmp.BitMap = bm;
    SetAPen(&tmp, 1);
    SetDrMd(&tmp, JAM1);
    Move(&tmp, 0, f->tf_Baseline);
    Text(&tmp, (CONST_STRPTR)str, (ULONG)len);
    WaitBlit();
    for (row = 0; row < th; row++) {
        const UBYTE* bits = (const UBYTE*)bm->Planes[0] + (size_t)row * bm->BytesPerRow;
        int px;
        for (px = 0; px < tw;) {
            int run;
            if (!(bits[px >> 3] & (0x80u >> (px & 7)))) {
                px++;
                continue;
            }
            run = px + 1;
            while (run < tw && (bits[run >> 3] & (0x80u >> (run & 7)))) {
                run++;
            }
            aa_rect(c, rp, x + px, y + row, x + run - 1, y + row, fg);
            px = run;
        }
    }
    FreeBitMap(bm);
}

int aa_text_width(aa_ctx* c, Object* obj, const char* str) {
    int w;
    struct TTE_Pixmap* pm;
    if (c->backend == AA_PLAIN) {
        struct RastPort tmp;
        InitRastPort(&tmp);
        SetFont(&tmp, aa_plain_textfont(c, obj));
        if (c->plain_bold) {
            SetSoftStyle(&tmp, FSF_BOLD, AskSoftStyle(&tmp));
        }
        return (int)TextLength(&tmp, (CONST_STRPTR)str, (ULONG)strlen(str)) + (c->plain_bold ? 1 : 0);
    }
    pm = tte_pixmap(c->font, str, (ULONG)strlen(str));
    w = pm != NULL ? (int)pm->width : 0;
    if (pm != NULL) {
        tte_free_pixmap(pm);
    }
    return w;
}

int aa_text_height(aa_ctx* c, Object* obj) {
    return c->backend == AA_PLAIN ? (int)aa_plain_textfont(c, obj)->tf_YSize : c->font_px + c->font_px / 4;
}

void aa_text(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int len = (int)strlen(str);
    if (c->backend == AA_PLAIN && c->tc) {
        plain_text_tc(c, obj, rp, x, y, str, fg);
        return;
    }
    if (!mui_theme_text_uses_token(c->backend == AA_TC, c->backend == AA_PLAIN, fg, bg)) {
        LONG pen = c->backend == AA_PLAIN ? aa_pen(c, fg) : (LONG)_dri(obj)->dri_Pens[TEXTPEN];
        struct TextFont* tf = c->backend == AA_PLAIN ? aa_plain_textfont(c, obj) : _font(obj);
        SetFont(rp, tf);
        SetSoftStyle(rp, c->backend == AA_PLAIN && c->plain_bold ? FSF_BOLD : FS_NORMAL, AskSoftStyle(rp));
        SetAPen(rp, pen);
        SetDrMd(rp, JAM1);
        Move(rp, x, y + tf->tf_Baseline);
        Text(rp, (CONST_STRPTR)str, (ULONG)len);
        return;
    }
    if (c->backend == AA_TC) {
        long asc = 0;
        struct TagItem set[5] = {{TTE_Screen, (ULONG)c->scr}, {TTE_Antialias, 2}, {TTE_Foreground, fg}, {TTE_Background, bg}, {TAG_DONE, 0}};
        struct TagItem get[2] = {{TTE_FontAscender, (ULONG)&asc}, {TAG_DONE, 0}};
        SetDrMd(rp, JAM1);
        tte_set_font(rp, c->font);
        tte_set_attrs(rp, set);
        tte_get_attrs(rp, get);
        Move(rp, x, y + (int)asc);
        tte_text(rp, str, (ULONG)len);
    } else {
        struct TTE_Pixmap* pm = tte_pixmap(c->font, str, (ULONG)len);
        uint32_t ramp[MUI_AA_RAMP_MAX];
        LONG pens[MUI_AA_RAMP_MAX];
        int py, px, i, slots;
        if (pm == NULL) {
            return;
        }
        slots = mui_aa_ramp_pens(c->kind, c->tok, fg, bg, ramp);
        for (i = 0; i < slots; i++) {
            pens[i] = aa_pen(c, ramp[i]);
        }
        for (py = 0; py < (int)pm->height; py++) {
            const UBYTE* row = pm->data + (size_t)py * pm->width;
            for (px = 0; px < (int)pm->width;) {
                int idx = mui_aa_ramp_slot(c->kind, row[px]), run = px + 1;
                while (run < (int)pm->width && mui_aa_ramp_slot(c->kind, row[run]) == idx) {
                    run++;
                }
                if (idx > 0 && idx < slots && pens[idx] >= 0) {
                    SetAPen(rp, pens[idx]);
                    RectFill(rp, x + px, y + py, x + run - 1, y + py);
                }
                px = run;
            }
        }
        tte_free_pixmap(pm);
    }
}

void aa_free(void) {
    if (TTEngineBase != NULL) {
        CloseLibrary(TTEngineBase);
        TTEngineBase = NULL;
    }
    if (g_p96 != NULL) {
        CloseLibrary(g_p96);
        g_p96 = NULL;
    }
    if (g_cgx != NULL) {
        CloseLibrary(g_cgx);
        g_cgx = NULL;
    }
    g_rtg_tried = 0;
}

static void ctx_release_pens(aa_ctx* c) {
    int i;
    if (c->cm != NULL) {
        for (i = 0; i < c->npens; i++) {
            if (c->pen[i] >= 0) {
                ReleasePen(c->cm, c->pen[i]);
            }
        }
    }
    c->npens = 0;
}

static void ctx_retheme(aa_ctx* c) {
    ctx_release_pens(c);
    memcpy(c->tok, mui_theme_tokens(), sizeof c->tok);
    c->bg_rgb = ctx_bg(c);
    if (aa_live(c->obj)) {
        MUI_Redraw(c->obj, MADF_DRAWOBJECT);
    }
}

uint32_t aa_retheme_all(void) {
    aa_ctx* c;
    uint32_t n = 0;
    aatip_drop(NULL); /* I731: an open tip holds pens of the old palette */
    for (c = g_ctx_list; c != NULL; c = c->next) {
        ctx_retheme(c);
        n++;
    }
    return n;
}

#ifdef UP_CLASS_LIBRARY
/* I685n: runs on the application's task (MUIM_Application_PushMethod + MUIM_CallHook from up-mcc.library). msg[0] is the key given to
 * UP_ThemeAttach, the context. The method may arrive after the object left the window, so the context is looked up in this
 * library's own list first; a gone context is ignored. */
static ULONG retheme_hook_fn(struct Hook* hook, APTR app, ULONG* msg) {
    aa_ctx* c;
    (void)hook;
    (void)app;
    aatip_drop(NULL);
    for (c = g_ctx_list; c != NULL; c = c->next) {
        if ((ULONG)(uintptr_t)c == msg[0]) {
            ctx_retheme(c);
            break;
        }
    }
    return 0;
}

#endif
