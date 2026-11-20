/* Selecta_Canvas (MUI-P5-1, MUI-P5-2). MUI 3.8 only (muimaster v19): MUI_CreateCustomClass on Area. Setup opens
 * Picasso96API.library for the pixel push (the Replay has no cybergraphics.library, same as the desktop RTG backend),
 * Show sizes the canvas to the object, Draw renders the grid with src/gui/draw and writes it to the window RastPort inside
 * the object rectangle, Hide and Cleanup release. Planar screens have no Picasso96: Draw then leaves the object blank
 * until MUI-P5-7 draws 1-bit covers there. */
#include "mui_canvas_class.h"
#include "map/mui_grid_art.h"

#include <selecta/gui/gui_draw.h>
#include <selecta/platform.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <devices/inputevent.h>
#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <inline/macros.h>
#include "amiga/p96_write.h"
#include <exec/execbase.h>
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
#define CANVAS_QN 8u

typedef struct canvas_data {
    mui_canvas* mc;
    mui_grid_style style;
    int rtg;
    struct Library* p96;
    const mui_tile* tiles;
    uint32_t n, cursor;
    uint32_t sig;      /* I1473g4: mui_grid_signature of the last set_grid; an equal one skips the redraw */
    int sig_set;
    uint32_t first_row; /* first tile row of the last grid draw, for the captions */
    mui_art_fn art;
    void* art_ctx;
    uint32_t* img;   /* I1440q3: real cover words (img_edge squared), owned; NULL = the generated cover */
    int32_t img_edge;
    LONG pens[MUI_COVER_CUBE]; /* I1440q4: AGA, the cube's shared pens, held from the first image draw to MUIM_Cleanup */
    int pens_held;
    mui_aga_cache* aga; /* I1473g5: grid cover indices (cube, pen-independent), one per art id */
    uint8_t* idx;     /* the remapped image (img_edge squared); NULL = redo from img */
    uint8_t* chunky;  /* the scaled, pen-mapped pixels of the last draw (cw squared) */
    int32_t cw;
    int single;      /* I1399d: one cover over the whole object (Now Playing art), not a grid */
    uint32_t kind, tint; /* tint: 0x00RRGGBB of the kind token, the planar fill */
    char title[64], ext[12];
    int lead;                         /* I1455: download spinner, the lead dot 0..7; -1 = no spinner */
    uint32_t spin_accent, spin_muted; /* 0x00RRGGBB */
    /* I1473g7: input. The handler runs while the canvas is shown; events wait in a ring until the app takes them (ReturnID input_id). */
    struct MUI_EventHandlerNode ehn;
    int ehn_on;
    ULONG input_id;
    mui_grid_input q[CANVAS_QN];
    uint32_t q_head, q_n;
    ULONG click_s, click_m; /* time of the last left click and its tile, for the double click */
    int32_t click_tile;
    /* I1473g8b2: Cover Flow on RTG. glide_from is the 16.16 centre the glide starts at; glide_ms0 the start in ms (0 = at rest). */
    int flow, glides;
    int64_t glide_from;
    uint32_t glide_ms0;
    int32_t grid_edge; /* the grid style edge, kept while Cover Flow runs at the loader edge */
    mui_flow_stats fs;
    mui_flow_view fv; /* the layout of the last flow draw (hit tests and captions) */
} canvas_data;

/* Build-time guard: BOOPSI keeps the instance size in a UWORD (cl_InstSize). */
typedef char canvas_data_fits_uword[(sizeof(canvas_data) < 65536u) ? 1 : -1];

static struct MUI_CustomClass* g_class;

static uint32_t canvas_now_ms(void) {
    ULONG sec, mic;
    CurrentTime(&sec, &mic);
    return (uint32_t)(sec * 1000UL + mic / 1000UL);
}

/* The fractional centre now: the cursor at rest, else gliding from glide_from (the Mac I400 rule, shared sel_uim_flow_centre_q16). */
static int64_t canvas_flow_centre(const canvas_data* d, int* gliding) {
    int32_t g = 65536;
    if (d->glides && d->glide_ms0 != 0u) {
        g = mui_flow_glide_q16(canvas_now_ms() - d->glide_ms0);
    }
    *gliding = g < 65536;
    return sel_uim_flow_centre_q16(d->glide_from, d->cursor, g);
}

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
        d->lead = -1;
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
    free(d->idx);
    free(d->chunky);
    d->idx = d->chunky = NULL;
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

/* I1473g4: title (text pen) and artist (shadow pen, the muted line) under each drawn cover, Text() in the window font like canvas_label,
 * after the canvas push. Tiles cut by the object edge get no caption (the push already clipped their card). */
static void canvas_captions(Object* obj, const canvas_data* d) {
    struct RastPort* rp = _rp(obj);
    LONG h = (LONG)rp->TxHeight, base = (LONG)rp->TxBaseline;
    uint32_t i, cols = mui_grid_cols(&d->style, (int32_t)_mwidth(obj));
    int32_t shift = (int32_t)d->first_row * (d->style.edge + 2 * d->style.pad + d->style.caption + d->style.gap);
    if (d->style.caption <= 0 || _dri(obj) == NULL) {
        return;
    }
    SetDrMd(rp, JAM1);
    for (i = (uint32_t)d->first_row * cols; i < d->n; i++) {
        sel_gui_rect t = mui_grid_tile_rect(&d->style, (int32_t)_mwidth(obj), i);
        LONG x = _mleft(obj) + t.x + d->style.pad, y = _mtop(obj) + (t.y - shift) + d->style.pad + d->style.edge + 1;
        struct TextExtent te;
        const char* line[2];
        uint32_t k;
        if (t.y - shift + t.h > (int32_t)_mheight(obj)) {
            break;
        }
        line[0] = d->tiles[i].title;
        line[1] = d->tiles[i].artist;
        for (k = 0; k < 2u; k++) {
            LONG len = line[k] != NULL ? (LONG)strlen(line[k]) : 0, fit;
            if (len == 0) {
                continue;
            }
            fit = (LONG)TextFit(rp, (CONST_STRPTR)line[k], (ULONG)len, &te, NULL, 1, (LONG)d->style.edge, h + 2);
            SetAPen(rp, _dri(obj)->dri_Pens[k == 0u ? TEXTPEN : SHADOWPEN]);
            Move(rp, x, y + (LONG)k * h + base);
            Text(rp, (CONST_STRPTR)line[k], (ULONG)fit);
        }
    }
}

/* I1440q4: drop the pen-bound caches; held pens go back to the screen (cm NULL: the screen is gone, nothing to release). */
static void canvas_aga_free(canvas_data* d, struct ColorMap* cm) {
    uint32_t i;
    if (d->pens_held && cm != NULL) {
        for (i = 0; i < MUI_COVER_CUBE; i++) {
            if (d->pens[i] >= 0) {
                ReleasePen(cm, (ULONG)d->pens[i]);
            }
        }
    }
    d->pens_held = 0;
    mui_aga_cache_close(d->aga);
    d->aga = NULL;
    free(d->idx);
    free(d->chunky);
    d->idx = d->chunky = NULL;
    d->cw = 0;
}

/* The cube's pens, obtained once per object and held to MUIM_Cleanup. */
static void canvas_aga_pens(struct ColorMap* cm, canvas_data* d) {
    uint32_t i;
    if (d->pens_held) {
        return;
    }
    for (i = 0; i < MUI_COVER_CUBE; i++) {
        uint32_t c = mui_cover_cube_rgb(i);
        d->pens[i] = ObtainBestPenA(cm, ((c >> 16) & 0xFFu) * 0x01010101u, ((c >> 8) & 0xFFu) * 0x01010101u, (c & 0xFFu) * 0x01010101u, NULL);
    }
    d->pens_held = 1;
}

/* I1440q4: the real cover on a palette screen: cube pens, ordered-dither index image (once per image), nearest scale to the largest
 * centred square (once per size), WriteChunkyPixels (graphics v40). Returns 0 when it cannot (OS 3.0, no memory): the tint path draws. */
static int canvas_aga_image(Object* obj, canvas_data* d) {
    struct ColorMap* cm = _screen(obj)->ViewPort.ColorMap;
    LONG side = _mwidth(obj) < _mheight(obj) ? _mwidth(obj) : _mheight(obj);
    LONG x0 = _mleft(obj) + (_mwidth(obj) - side) / 2, y0 = _mtop(obj) + (_mheight(obj) - side) / 2;
    uint32_t i;
    if (d->img == NULL || GfxBase->LibNode.lib_Version < 40 || side < 8) {
        return 0;
    }
    canvas_aga_pens(cm, d);
    if (d->idx == NULL) {
        d->idx = (uint8_t*)malloc((size_t)d->img_edge * (size_t)d->img_edge);
        if (d->idx == NULL || !mui_cover_remap(d->img, d->img_edge, d->idx)) {
            free(d->idx);
            d->idx = NULL;
            return 0;
        }
    }
    if (d->chunky == NULL || d->cw != side) {
        uint32_t n = (uint32_t)side * (uint32_t)side;
        free(d->chunky);
        d->chunky = (uint8_t*)malloc(n);
        if (d->chunky == NULL) {
            d->cw = 0;
            return 0;
        }
        mui_cover_scale8(d->idx, d->img_edge, d->chunky, side, side);
        for (i = 0; i < n; i++) {
            LONG p = d->pens[d->chunky[i]];
            d->chunky[i] = (uint8_t)(p < 0 ? 0 : p);
        }
        d->cw = side;
    }
    WriteChunkyPixels(_rp(obj), x0, y0, x0 + side - 1, y0 + side - 1, d->chunky, side);
    return 1;
}

/* 0x00RRGGBB as the nearest palette pen; the caller releases it (-1 when the screen has none left). */
static LONG canvas_pen(struct ColorMap* cm, uint32_t rgb) {
    return ObtainBestPenA(cm, ((rgb >> 16) & 0xFFu) * 0x01010101u, ((rgb >> 8) & 0xFFu) * 0x01010101u, (rgb & 0xFFu) * 0x01010101u, NULL);
}

/* I1455: the download spinner on a planar screen: eight pen-filled dots on the ring (sel_gui_spinner geometry), the lead dot in the
 * accent pen. The two pens are held for this draw only, so no pen outlives a redraw. */
static void canvas_spin_aga(Object* obj, const canvas_data* d) {
    struct ColorMap* cm;
    LONG side = _mwidth(obj) < _mheight(obj) ? _mwidth(obj) : _mheight(obj);
    int32_t cx = (int32_t)(_mleft(obj) + _mwidth(obj) / 2), cy = (int32_t)(_mtop(obj) + _mheight(obj) / 2);
    LONG pa, pm;
    uint32_t i;
    if (d->lead < 0 || side < 8) {
        return;
    }
    cm = _screen(obj)->ViewPort.ColorMap;
    pa = canvas_pen(cm, d->spin_accent);
    pm = canvas_pen(cm, d->spin_muted);
    for (i = 0; i < 8u && pa >= 0 && pm >= 0; i++) {
        int32_t x, y, dot = sel_gui_spinner_dot(i, cx, cy, (int32_t)(side / 8), &x, &y);
        SetAPen(_rp(obj), (ULONG)(i == (uint32_t)d->lead ? pa : pm));
        RectFill(_rp(obj), x - dot / 2, y - dot / 2, x - dot / 2 + dot - 1, y - dot / 2 + dot - 1);
    }
    if (pa >= 0) ReleasePen(cm, (ULONG)pa);
    if (pm >= 0) ReleasePen(cm, (ULONG)pm);
}

/* I1473g5: the AGA grid sink. Everything is clipped to the object (the card rows run past the bottom edge), RectFill in three pens
 * held for the draw, covers through the cube pens one WriteChunkyPixels per tile. */
typedef struct aga_sink_ctx {
    Object* obj;
    canvas_data* d;
    struct ColorMap* cm;
    uint32_t rgb[3];
    LONG pen[3];
} aga_sink_ctx;

static int aga_clip(const aga_sink_ctx* c, sel_gui_rect r, LONG* x0, LONG* y0, LONG* x1, LONG* y1) {
    LONG l = _mleft(c->obj), t = _mtop(c->obj);
    *x0 = l + (r.x > 0 ? r.x : 0);
    *y0 = t + (r.y > 0 ? r.y : 0);
    *x1 = l + r.x + r.w - 1 < _mright(c->obj) ? l + r.x + r.w - 1 : _mright(c->obj);
    *y1 = t + r.y + r.h - 1 < _mbottom(c->obj) ? t + r.y + r.h - 1 : _mbottom(c->obj);
    return *x0 <= *x1 && *y0 <= *y1;
}

static void aga_fill(const aga_sink_ctx* c, sel_gui_rect r, LONG pen) {
    LONG x0, y0, x1, y1;
    if (pen >= 0 && aga_clip(c, r, &x0, &y0, &x1, &y1)) {
        SetAPen(_rp(c->obj), (ULONG)pen);
        RectFill(_rp(c->obj), x0, y0, x1, y1);
    }
}

/* Background, card and outline colours take one of three pens each, obtained on first use and released after the draw. */
static void aga_rect(void* ctx, sel_gui_rect r, uint32_t rgb) {
    aga_sink_ctx* c = ctx;
    uint32_t k;
    for (k = 0; k < 3u && c->pen[k] >= 0 && c->rgb[k] != rgb; k++) {}
    if (k == 3u) {
        return;
    }
    if (c->pen[k] < 0) {
        c->pen[k] = canvas_pen(c->cm, rgb);
        c->rgb[k] = rgb;
    }
    aga_fill(c, r, c->pen[k]);
}

static void aga_cover(void* ctx, sel_gui_rect r, const uint8_t* idx, uint32_t tint) {
    aga_sink_ctx* c = ctx;
    canvas_data* d = c->d;
    LONG x0, y0, x1, y1;
    if (idx == NULL || d->chunky == NULL) {
        LONG pen = canvas_pen(c->cm, tint); /* no art yet: the kind tint, a pen held for this one fill */
        aga_fill(c, r, pen);
        if (pen >= 0) {
            ReleasePen(c->cm, (ULONG)pen);
        }
        return;
    }
    if (aga_clip(c, r, &x0, &y0, &x1, &y1)) {
        LONG e = (LONG)d->style.edge, i, n = e * e;
        for (i = 0; i < n; i++) {
            LONG p = d->pens[idx[i]];
            d->chunky[i] = (uint8_t)(p < 0 ? 0 : p);
        }
        WriteChunkyPixels(_rp(c->obj), x0, y0, x1, y1, d->chunky + (y0 - (_mtop(c->obj) + r.y)) * e + (x0 - (_mleft(c->obj) + r.x)), e);
    }
}

/* I1473g8b3: one flow strip (indices already scaled and dimmed) through the cube pens, clipped to the object. */
static void aga_chunky(void* ctx, sel_gui_rect r, const uint8_t* idx) {
    aga_sink_ctx* c = ctx;
    canvas_data* d = c->d;
    LONG x0, y0, x1, y1;
    if (d->chunky != NULL && aga_clip(c, r, &x0, &y0, &x1, &y1)) {
        LONG i, n = (LONG)r.w * (LONG)r.h;
        for (i = 0; i < n; i++) {
            LONG p = d->pens[idx[i]];
            d->chunky[i] = (uint8_t)(p < 0 ? 0 : p);
        }
        WriteChunkyPixels(_rp(c->obj), x0, y0, x1, y1, d->chunky + (y0 - (_mtop(c->obj) + r.y)) * r.w + (x0 - (_mleft(c->obj) + r.x)), r.w);
    }
}

static void canvas_draw_grid_aga(Object* obj, canvas_data* d) {
    static const mui_aga_sink sink = {aga_rect, aga_cover, NULL};
    aga_sink_ctx c;
    uint32_t k;
    struct ColorMap* cm = _screen(obj)->ViewPort.ColorMap;
    c.obj = obj;
    c.d = d;
    c.cm = cm;
    for (k = 0; k < 3u; k++) {
        c.pen[k] = -1;
        c.rgb[k] = 0;
    }
    d->style.caption = 2 * (LONG)_rp(obj)->TxHeight + 2;
    d->first_row = mui_grid_first_row(&d->style, (int32_t)_mwidth(obj), (int32_t)_mheight(obj), d->cursor);
    if (GfxBase->LibNode.lib_Version >= 40) {
        canvas_aga_pens(cm, d);
        if (d->aga == NULL) {
            d->aga = mui_aga_cache_open(d->style.edge, 24);
        }
        if (d->chunky == NULL || d->cw != d->style.edge) {
            free(d->chunky);
            d->chunky = (uint8_t*)malloc((size_t)d->style.edge * (size_t)d->style.edge);
            d->cw = d->chunky != NULL ? d->style.edge : 0;
        }
    } /* OS below v40: no cache, so every cover paints its tint */
    mui_canvas_draw_grid_aga(&d->style, (int32_t)_mwidth(obj), (int32_t)_mheight(obj), d->tiles, d->n, d->cursor, d->first_row, d->art,
                             d->art_ctx, d->chunky != NULL ? d->aga : NULL, &sink, &c);
    for (k = 0; k < 3u; k++) {
        if (c.pen[k] >= 0) {
            ReleasePen(cm, (ULONG)c.pen[k]);
        }
    }
    canvas_captions(obj, d);
}

/* I1473g8b2: the front cover's title (text pen) and artist (shadow pen), each centred in the caption block with TextFit. */
static void canvas_flow_captions(Object* obj, const canvas_data* d) {
    struct RastPort* rp = _rp(obj);
    LONG h = (LONG)rp->TxHeight, base = (LONG)rp->TxBaseline;
    const mui_tile* t;
    const char* line[2];
    uint32_t k;
    if (d->tiles == NULL || d->cursor >= d->n || d->fv.caption.h <= 0 || _dri(obj) == NULL) {
        return;
    }
    t = &d->tiles[d->cursor];
    line[0] = t->title;
    line[1] = t->artist;
    SetDrMd(rp, JAM1);
    for (k = 0; k < 2u; k++) {
        struct TextExtent te;
        LONG len = line[k] != NULL ? (LONG)strlen(line[k]) : 0, fit, w;
        if (len == 0) {
            continue;
        }
        fit = (LONG)TextFit(rp, (CONST_STRPTR)line[k], (ULONG)len, &te, NULL, 1, (LONG)d->fv.caption.w, h + 2);
        w = (LONG)te.te_Extent.MaxX - (LONG)te.te_Extent.MinX + 1;
        SetAPen(rp, _dri(obj)->dri_Pens[k == 0u ? TEXTPEN : SHADOWPEN]);
        Move(rp, _mleft(obj) + d->fv.caption.x + (d->fv.caption.w - w) / 2, _mtop(obj) + d->fv.caption.y + (LONG)k * h + base);
        Text(rp, (CONST_STRPTR)line[k], (ULONG)fit);
    }
}

/* I1473g8b3: Cover Flow on a palette screen. Index cache of 12 covers at the loader edge; OS below v40 shows dimmed tints. */
static void canvas_draw_flow_aga(Object* obj, canvas_data* d) {
    static const mui_aga_sink sink = {aga_rect, aga_cover, aga_chunky};
    aga_sink_ctx c;
    uint32_t k;
    int gliding;
    int64_t centre = canvas_flow_centre(d, &gliding);
    struct ColorMap* cm = _screen(obj)->ViewPort.ColorMap;
    c.obj = obj;
    c.d = d;
    c.cm = cm;
    for (k = 0; k < 3u; k++) {
        c.pen[k] = -1;
        c.rgb[k] = 0;
    }
    d->style.caption = 2 * (LONG)_rp(obj)->TxHeight + 2;
    if (GfxBase->LibNode.lib_Version >= 40) {
        mui_flow_view v;
        mui_flow_layout(&d->style, (int32_t)_mwidth(obj), (int32_t)_mheight(obj), centre, d->n, &v);
        canvas_aga_pens(cm, d);
        if (d->aga == NULL) {
            d->aga = mui_aga_cache_open(d->style.edge, mui_flow_art_slots(24u, 1));
        }
        if (d->chunky == NULL || d->cw != v.big) {
            free(d->chunky);
            d->chunky = (uint8_t*)malloc((size_t)v.big * (size_t)v.big);
            d->cw = d->chunky != NULL ? v.big : 0;
        }
    }
    mui_canvas_draw_flow_aga(&d->style, (int32_t)_mwidth(obj), (int32_t)_mheight(obj), d->tiles, d->n, centre, d->art, d->art_ctx,
                             d->chunky != NULL ? d->aga : NULL, &sink, &c, &d->fv);
    for (k = 0; k < 3u; k++) {
        if (c.pen[k] >= 0) {
            ReleasePen(cm, (ULONG)c.pen[k]);
        }
    }
    canvas_flow_captions(obj, d);
    if (!gliding) {
        d->glide_ms0 = 0u;
    }
}

static ULONG canvas_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    canvas_data* d = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & MADF_DRAWOBJECT) == 0) {
        return 0;
    }
    if (d->single && !d->rtg) { /* I1440q4: the real cover through the cube pens; else the kind tint as the nearest palette pen, then the label and title */
        if (!canvas_aga_image(obj, d)) {
            struct ColorMap* cm = _screen(obj)->ViewPort.ColorMap;
            LONG pen = canvas_pen(cm, d->tint);
            SetAPen(_rp(obj), (ULONG)pen);
            RectFill(_rp(obj), _mleft(obj), _mtop(obj), _mright(obj), _mbottom(obj));
            ReleasePen(cm, (ULONG)pen);
            canvas_label(obj, d);
        }
        canvas_spin_aga(obj, d);
        return 0;
    }
    if (!d->rtg) { /* I1473g5: the planar grid; no tiles set = blank as before */
        if (d->tiles != NULL && d->flow) {
            uint32_t t0 = canvas_now_ms();
            canvas_draw_flow_aga(obj, d);
            mui_flow_stats_draw(&d->fs, d->glide_ms0 != 0u, canvas_now_ms() - t0);
        } else if (d->tiles != NULL) {
            canvas_draw_grid_aga(obj, d);
        }
        return 0;
    }
    if (d->p96 == NULL) {
        return 0;
    }
    if (mui_canvas_fit(d->mc, (sel_gui_rect){_mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj)})) {
        if (d->single) {
            mui_canvas_draw_art(d->mc, (sel_gui_rect){0, 0, d->mc->c.w, d->mc->c.h}, d->img != NULL ? MUI_ART_DATATYPE : MUI_ART_COVER, d->img,
                                d->img_edge, d->img_edge, d->kind, d->ext, d->title);
            if (d->lead >= 0 && d->mc->c.w >= 8 && d->mc->c.h >= 8) { /* I1455: the download spinner, centred on the art */
                int32_t side = d->mc->c.w < d->mc->c.h ? d->mc->c.w : d->mc->c.h;
                sel_gui_spinner(&d->mc->c, d->mc->c.w / 2, d->mc->c.h / 2, side / 8, (uint32_t)d->lead, 0xFF000000u | d->spin_accent, 0xFF000000u | d->spin_muted);
            }
            mui_canvas_push(d->p96, d->mc->pix, d->mc->c.w, d->mc->c.h, _rp(obj), _mleft(obj), _mtop(obj));
            canvas_label(obj, d);
            return 0;
        }
        if (d->flow && d->tiles != NULL) { /* I1473g8b2: Cover Flow; a mid-glide frame keeps the redraw tick alive (mui_canvas_flow_tick) */
            int gliding;
            int64_t c = canvas_flow_centre(d, &gliding);
            uint32_t t0 = canvas_now_ms();
            d->style.caption = 2 * (int32_t)_rp(obj)->TxHeight + 2;
            mui_canvas_draw_flow(d->mc, &d->style, d->tiles, d->n, c, d->art, d->art_ctx, &d->fv);
            mui_canvas_push(d->p96, d->mc->pix, d->mc->c.w, d->mc->c.h, _rp(obj), _mleft(obj), _mtop(obj));
            canvas_flow_captions(obj, d);
            mui_flow_stats_draw(&d->fs, gliding, canvas_now_ms() - t0);
            if (!gliding) {
                d->glide_ms0 = 0u;
            }
            return 0;
        }
        d->style.caption = d->tiles != NULL ? 2 * (int32_t)_rp(obj)->TxHeight + 2 : 0; /* two caption lines of the window font */
        d->first_row = mui_grid_first_row(&d->style, d->mc->c.w, d->mc->c.h, d->cursor);
        mui_canvas_draw_grid(d->mc, &d->style, d->tiles, d->n, d->cursor, d->first_row, d->art, d->art_ctx);
        mui_canvas_push(d->p96, d->mc->pix, d->mc->c.w, d->mc->c.h, _rp(obj), _mleft(obj), _mtop(obj));
        canvas_captions(obj, d);
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

/* I1473g7: arrows, Enter and the wheel while the canvas is shown (not while a text gadget has the keys); left click picks a tile,
 * a double click opens it, right click picks it and leaves the event for the context menu. */
static void canvas_post(Object* obj, canvas_data* d, uint32_t kind, int32_t arg) {
    if (d->q_n < CANVAS_QN) {
        d->q[(d->q_head + d->q_n++) % CANVAS_QN] = (mui_grid_input){kind, arg};
    }
    DoMethod(_app(obj), MUIM_Application_ReturnID, d->input_id);
}

static ULONG canvas_event(Object* obj, canvas_data* d, const struct IntuiMessage* im) {
    int inside = im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj);
    mui_grid_input in;
    if (d->single || d->tiles == NULL) {
        return 0;
    }
    if (im->Class == IDCMP_RAWKEY) {
        Object* act = NULL;
        ULONG text = 0;
        uint32_t code = im->Code;
        if ((code & IECODE_UP_PREFIX) != 0 || (im->Qualifier & (IEQUALIFIER_CONTROL | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND | IEQUALIFIER_LALT | IEQUALIFIER_RALT)) != 0) {
            return 0;
        }
        GetAttr(MUIA_Window_ActiveObject, _win(obj), (ULONG*)&act);
        if (act != NULL && GetAttr(MUIA_String_Contents, act, &text) != 0) {
            return 0; /* a text gadget owns the arrows */
        }
        if ((code == MUI_RAWKEY_WHEEL_UP || code == MUI_RAWKEY_WHEEL_DOWN) && !inside) {
            return 0;
        }
        if (!mui_grid_key(code, d->flow ? 0u : mui_grid_cols(&d->style, (int32_t)_mwidth(obj)), &in)) { /* Cover Flow has no rows: Up/Down pass, Left/Right/wheel step one */
            return 0;
        }
        canvas_post(obj, d, in.kind, in.arg);
        return MUI_EventHandlerRC_Eat;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS && inside && (im->Code == IECODE_LBUTTON || im->Code == IECODE_RBUTTON)) {
        int32_t t = d->flow ? sel_uim_flow_hit(d->fv.cv, d->fv.n, im->MouseX - _mleft(obj), im->MouseY - _mtop(obj), 0)
                            : mui_grid_hit(&d->style, (int32_t)_mwidth(obj), (int32_t)_mheight(obj), d->n, d->cursor, im->MouseX - _mleft(obj), im->MouseY - _mtop(obj));
        if (t < 0) {
            return 0;
        }
        if (im->Code == IECODE_RBUTTON) {
            canvas_post(obj, d, MUI_GIN_PICK, t);
            return 0;
        }
        if (d->flow && im->Code == IECODE_LBUTTON && (uint32_t)t != d->cursor) { /* D-F3: a side cover moves the flow to it; the front cover keeps the Grid click */
            d->click_tile = -1;
            canvas_post(obj, d, MUI_GIN_PICK, t);
            return MUI_EventHandlerRC_Eat;
        }
        if (d->click_tile == t && DoubleClick(d->click_s, d->click_m, im->Seconds, im->Micros)) {
            d->click_tile = -1;
            canvas_post(obj, d, MUI_GIN_OPEN, t);
        } else {
            d->click_tile = t;
            d->click_s = im->Seconds;
            d->click_m = im->Micros;
            canvas_post(obj, d, MUI_GIN_PICK, t);
        }
        return MUI_EventHandlerRC_Eat;
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
        canvas_aga_free(d, _screen(obj) != NULL ? _screen(obj)->ViewPort.ColorMap : NULL);
        if (d->p96 != NULL) {
            CloseLibrary(d->p96);
            d->p96 = NULL;
        }
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show:
        d = INST_DATA(cl, obj);
        mui_canvas_fit(d->mc, (sel_gui_rect){_mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj)});
        if (d->input_id != 0 && !d->ehn_on) {
            d->ehn.ehn_Priority = 1; /* above the list and the global key handler (-1): the canvas is the shown view */
            d->ehn.ehn_Flags = 0;
            d->ehn.ehn_Object = obj;
            d->ehn.ehn_Class = cl;
            d->ehn.ehn_Events = IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS;
            DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
            d->ehn_on = 1;
        }
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_HandleEvent:
        d = INST_DATA(cl, obj);
        if (((struct MUIP_HandleEvent*)msg)->imsg != NULL) {
            ULONG rc = canvas_event(obj, d, ((struct MUIP_HandleEvent*)msg)->imsg);
            if (rc != 0) {
                return rc;
            }
        }
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Hide:
        d = INST_DATA(cl, obj);
        if (d->ehn_on) {
            DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
            d->ehn_on = 0;
        }
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
    uint32_t sig = mui_grid_signature(tiles, n, cursor);
    int same = d->sig_set && sig == d->sig && tiles == d->tiles;
    d->sig = sig;
    d->sig_set = 1;
    if (d->flow && cursor != d->cursor && tiles != NULL) { /* a key mid-glide starts where the covers are (I400); steps on 020/030 */
        int gliding;
        d->glide_from = canvas_flow_centre(d, &gliding);
        d->glide_ms0 = d->glides ? canvas_now_ms() : 0u;
        if (d->glides) {
            mui_flow_stats_glide(&d->fs);
        }
    } else if (d->flow && d->glide_ms0 == 0u) {
        d->glide_from = (int64_t)cursor << 16;
    }
    d->tiles = tiles;
    d->n = n;
    d->cursor = cursor;
    d->art = art;
    d->art_ctx = art_ctx;
    if (!same) { /* I1473g4: redraw only on a signature change; art arrival is the caller's mui_canvas_invalidate */
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
}

void mui_canvas_set_flow(Object* obj, int on) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (on != 0 && !d->flow) {
        d->grid_edge = d->style.edge;
    }
    if ((on != 0) != d->flow) { /* the index cache is edge-sized: reopen at the new edge on the next draw */
        mui_aga_cache_close(d->aga);
        d->aga = NULL;
        free(d->chunky);
        d->chunky = NULL;
        d->cw = 0;
        d->style.edge = mui_flow_art_edge(d->grid_edge != 0 ? d->grid_edge : d->style.edge, on != 0);
    }
    d->flow = on != 0;
    d->glides = on != 0 && mui_flow_glides((SysBase->AttnFlags & AFF_68040) != 0); /* D-F2: 68040/68060 glide, 68020/68030 step */
    d->glide_ms0 = 0u;
    d->glide_from = (int64_t)d->cursor << 16;
    d->sig_set = 0;
}

int mui_canvas_flow_tick(Object* obj) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (!d->flow || d->glide_ms0 == 0u) {
        return 0;
    }
    MUI_Redraw(obj, MADF_DRAWOBJECT);
    return 1;
}

void mui_canvas_invalidate(Object* obj) {
    ((canvas_data*)INST_DATA(g_class->mcc_Class, obj))->sig_set = 0;
}

uint32_t mui_canvas_draws(Object* obj) {
    return ((canvas_data*)INST_DATA(g_class->mcc_Class, obj))->mc->draws;
}

void mui_canvas_grid_fit(Object* obj, uint32_t* cols, uint32_t* rows) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    LONG w = _mwidth(obj), h = _mheight(obj);
    *cols = w > 0 ? mui_grid_cols(&d->style, (int32_t)w) : 0u;
    *rows = h > 0 ? mui_grid_visible_rows(&d->style, (int32_t)h) : 0u;
}

void mui_canvas_input_set(Object* obj, ULONG return_id) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    d->input_id = return_id;
    d->click_tile = -1;
}

int mui_canvas_input_take(Object* obj, mui_grid_input* out) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (d->q_n == 0u) {
        return 0;
    }
    *out = d->q[d->q_head];
    d->q_head = (d->q_head + 1u) % CANVAS_QN;
    d->q_n--;
    return 1;
}

void mui_canvas_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}

Object* mui_canvas_cover_new(int screen_is_rtg) {
    static const mui_grid_style none = {0u, 0u, 0u, 0, 0, 0, 0, 0u};
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
    free(d->idx); /* I1440q4: a new image remaps; the held pens stay */
    free(d->chunky);
    d->idx = d->chunky = NULL;
    d->cw = 0;
    d->img = px != NULL ? malloc(n) : NULL;
    if (d->img != NULL) {
        memcpy(d->img, px, n);
    }
    d->img_edge = d->img != NULL ? edge : 0;
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

void mui_canvas_set_spin(Object* obj, int lead, uint32_t accent, uint32_t muted) {
    canvas_data* d = INST_DATA(g_class->mcc_Class, obj);
    if (d->lead == lead && d->spin_accent == accent && d->spin_muted == muted) {
        return;
    }
    d->lead = lead;
    d->spin_accent = accent;
    d->spin_muted = muted;
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

void mui_canvas_flow_stats(Object* obj, mui_flow_stats* out) {
    *out = ((canvas_data*)INST_DATA(g_class->mcc_Class, obj))->fs;
}
