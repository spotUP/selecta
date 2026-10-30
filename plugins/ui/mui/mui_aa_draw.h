/* mui_aa_draw (I667a, design 2026-10-28-mui-custom-classes.md section 2): the Amiga half shared by every Selecta_AA class.
 * Pen handling, rounded fills, and antialiased text with three backends picked once per object in aa_setup:
 *   AA_TC    screen depth 15 or more: ttengine TT_Text blends with RGB foreground/background;
 *   AA_PAL   palette screen: TT_GetPixmap coverage drawn as runs through ObtainBestPen ramp pens (AGA: four token pens);
 *   AA_PLAIN no ttengine or no Inter font: graphics Text() in the DrawInfo text pen.
 * The pure maths (states, ramps, metrics) live in map/mui_aa.c and are host tested. */
#ifndef MUI_AA_DRAW_H
#define MUI_AA_DRAW_H

#include <exec/types.h>
#include <graphics/rastport.h>
#include <libraries/mui.h>
#include <stdint.h>

#include "map/mui_aa.h"

enum { AA_PLAIN, AA_TC, AA_PAL };
#define AA_MAX_PENS 40

typedef struct aa_ctx {
    struct ColorMap* cm;
    struct Screen* scr;
    int backend, kind, font_px;
    APTR font;
    int npens;
    uint32_t pen_rgb[AA_MAX_PENS];
    LONG pen[AA_MAX_PENS];
    uint32_t tok[SEL_GUI_TOKEN_COUNT];
    Object* obj;           /* I676c: the owning class object, for the live THEME redraw */
    struct aa_ctx* next;   /* I676c: every set-up context is in one list (aa_setup .. aa_cleanup) */
    int linked;
} aa_ctx;

/* Backend, font, and the n colours in rgb[] as held pens. Call from MUIM_Setup after the super class. */
void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n);
/* Another font size on top of aa_setup (the title tone, base + 4); the bitmap tier and a failed open keep the base font. */
void aa_set_font_px(aa_ctx* c, int px);
/* I676c (live THEME): every set-up context drops its pens, copies the tokens in force (mui_theme_tokens), and its object redraws;
 * pens are re-obtained lazily by aa_pen at the next draw. Returns how many contexts were refreshed. */
uint32_t aa_retheme_all(void);
/* Releases every held pen and the font reference. Call from MUIM_Cleanup. */
void aa_cleanup(aa_ctx* c);
/* True when obj sits in an open window (between MUIM_Setup and MUIM_Cleanup). _win(obj) alone reads through the render
 * info, which is NULL before the first MUIM_Setup: that read returns whatever lies at address 0, and a redraw on that
 * garbage window crashed the Amiga (I684a, AACycle set before the window opened). Use this before every redraw from
 * OM_SET or an outside call. */
int aa_live(Object* obj);
/* Held pen for an RGB, obtained on first use; -1 when the table is full. */
LONG aa_pen(aa_ctx* c, uint32_t rgb);
/* Rows of RectFill with the insets of mui_aa_round_inset; radius 0 is one RectFill. */
void aa_round_fill(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen);
/* aa_round_fill that leaves the hole rectangle (hx, hy, hw, hh) untouched: for a Group subclass that paints after its super call
 * around a child the super already drew. */
void aa_round_fill_hole(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen, int hx, int hy, int hw, int hh);
/* Only the four corner wedges outside the rounded shape, in pen: rounds a box whose square face its background already filled. */
void aa_round_corners(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen);
/* One pixel rounded outline, same inset maths. */
void aa_round_frame(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen);
/* Caption pixel size. */
int aa_text_width(aa_ctx* c, Object* obj, const char* str);
int aa_text_height(aa_ctx* c, Object* obj);
/* Draw str with its top-left at x,y. fg and bg are 0xRRGGBB; an unreadable pair (contrast gate) uses the DrawInfo text pen. */
void aa_text(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg, uint32_t bg);
/* Process wide: closes ttengine at the last class free. */
void aa_free(void);

#endif
