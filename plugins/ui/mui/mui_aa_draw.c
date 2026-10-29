/* mui_aa_draw: see mui_aa_draw.h. The ttengine V6 calls come from the I665 spike (tags and LVOs from its Developer/include). */
#include "mui_aa_draw.h"

#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <graphics/text.h>
#include <inline/macros.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include <selecta/uimodel/theme.h>
#include "map/mui_theme.h"

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
static const char* const g_ttf[] = {"RAM:Inter-Regular.ttf", "Selecta:Fonts/Inter-Regular.ttf", "FONTS:Inter-Regular.ttf", NULL};
static int g_missing_logged;

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
        TTEngineBase = OpenLibrary((CONST_STRPTR) "ttengine.library", 6);
    }
    if (TTEngineBase != NULL && free_slot >= 0) {
        for (i = 0; g_ttf[i] != NULL; i++) {
            struct TagItem tl[4] = {{TTE_FontFile, (ULONG)g_ttf[i]}, {TTE_FontSize, (ULONG)px}, {TTE_Screen, (ULONG)scr}, {TAG_DONE, 0}};
            APTR f = tte_open_font(tl);
            if (f != NULL) {
                g_fonts[free_slot].px = px;
                g_fonts[free_slot].refs = 1;
                g_fonts[free_slot].font = f;
                return f;
            }
        }
    }
    if (!g_missing_logged) {
        g_missing_logged = 1;
        fprintf(stderr, "selecta: mui: ttengine or Inter missing, plain text\n");
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
    struct TagItem tl[2] = {{OBP_Precision, PRECISION_GUI}, {TAG_DONE, 0}};
    return ObtainBestPenA(cm, ((rgb >> 16) & 0xFFu) * 0x01010101UL, ((rgb >> 8) & 0xFFu) * 0x01010101UL, (rgb & 0xFFu) * 0x01010101UL, tl);
}

LONG aa_pen(aa_ctx* c, uint32_t rgb) {
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

void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n) {
    int i, depth;
    txt_theme th;
    memset(c, 0, sizeof *c);
    txt_theme_default(&th);
    memcpy(c->tok, th.token, sizeof c->tok);
    c->scr = _screen(obj);
    c->cm = c->scr->ViewPort.ColorMap;
    depth = (int)GetBitMapAttr(c->scr->RastPort.BitMap, BMA_DEPTH);
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
    for (i = 0; i < n; i++) {
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

int aa_live(Object* obj) {
    return obj != NULL && muiRenderInfo(obj) != NULL && _win(obj) != NULL;
}

void aa_cleanup(aa_ctx* c) {
    int i;
    if (c->cm != NULL) {
        for (i = 0; i < c->npens; i++) {
            if (c->pen[i] >= 0) {
                ReleasePen(c->cm, c->pen[i]);
            }
        }
    }
    c->npens = 0;
    c->cm = NULL;
    if (c->font != NULL) {
        font_put(c->font);
        c->font = NULL;
    }
}

void aa_round_fill(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen) {
    int row;
    SetAPen(rp, pen);
    if (radius <= 0) {
        RectFill(rp, x, y, x + w - 1, y + h - 1);
        return;
    }
    for (row = 0; row < h; row++) {
        int in = mui_aa_round_inset(w, h, radius, row);
        RectFill(rp, x + in, y + row, x + w - 1 - in, y + row);
    }
}

void aa_round_fill_hole(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen, int hx, int hy, int hw, int hh) {
    int row;
    SetAPen(rp, pen);
    for (row = 0; row < h; row++) {
        int in = radius > 0 ? mui_aa_round_inset(w, h, radius, row) : 0, l = x + in, r = x + w - 1 - in, py = y + row;
        if (py < hy || py >= hy + hh) {
            RectFill(rp, l, py, r, py);
            continue;
        }
        if (l < hx) {
            RectFill(rp, l, py, hx - 1 < r ? hx - 1 : r, py);
        }
        if (r >= hx + hw) {
            RectFill(rp, l > hx + hw ? l : hx + hw, py, r, py);
        }
    }
}

void aa_round_corners(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen) {
    int row;
    if (radius <= 0) {
        return;
    }
    SetAPen(rp, pen);
    for (row = 0; row < h; row++) {
        int in = mui_aa_round_inset(w, h, radius, row);
        if (in > 0) {
            RectFill(rp, x, y + row, x + in - 1, y + row);
            RectFill(rp, x + w - in, y + row, x + w - 1, y + row);
        }
    }
}

void aa_round_frame(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen) {
    int row;
    SetAPen(rp, pen);
    for (row = 0; row < h; row++) {
        int in = mui_aa_round_inset(w, h, radius, row);
        if (radius <= 0 || row == 0 || row == h - 1) {
            RectFill(rp, x + in, y + row, x + w - 1 - in, y + row);
        } else {
            /* Join to the neighbour row nearer the edge so the corner stays closed. */
            int up = mui_aa_round_inset(w, h, radius, row < h / 2 ? row - 1 : row + 1);
            int end = up - 1 > in ? up - 1 : in;
            RectFill(rp, x + in, y + row, x + end, y + row);
            RectFill(rp, x + w - 1 - end, y + row, x + w - 1 - in, y + row);
        }
    }
}

int aa_text_width(aa_ctx* c, Object* obj, const char* str) {
    int w;
    struct TTE_Pixmap* pm;
    if (c->backend == AA_PLAIN) {
        struct RastPort tmp;
        InitRastPort(&tmp);
        SetFont(&tmp, _font(obj));
        return (int)TextLength(&tmp, (CONST_STRPTR)str, (ULONG)strlen(str));
    }
    pm = tte_pixmap(c->font, str, (ULONG)strlen(str));
    w = pm != NULL ? (int)pm->width : 0;
    if (pm != NULL) {
        tte_free_pixmap(pm);
    }
    return w;
}

int aa_text_height(aa_ctx* c, Object* obj) {
    return c->backend == AA_PLAIN ? (int)_font(obj)->tf_YSize : c->font_px + c->font_px / 4;
}

void aa_text(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    int len = (int)strlen(str);
    if (!mui_theme_text_uses_token(c->backend == AA_TC, c->backend == AA_PLAIN, fg, bg)) {
        LONG pen = c->backend == AA_PLAIN ? aa_pen(c, fg) : (LONG)_dri(obj)->dri_Pens[TEXTPEN];
        SetFont(rp, _font(obj));
        SetAPen(rp, pen);
        SetDrMd(rp, JAM1);
        Move(rp, x, y + _font(obj)->tf_Baseline);
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
}
