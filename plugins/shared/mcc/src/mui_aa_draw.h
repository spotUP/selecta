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
    int tc;                /* I685w: 15+ bit screen with an RTG fill library open: direct RGB fills, no pens */
    uint32_t bg_rgb;       /* I685w: what aa_erase fills on tc; the Background token unless the class sets another */
    int aspect_q8;         /* I685e1: pixel height over width x 256 of this object's screen (DrawInfo resolution), 256..512 */
    APTR font;
    int npens;
    uint32_t pen_rgb[AA_MAX_PENS];
    LONG pen[AA_MAX_PENS];
    uint32_t tok[SEL_GUI_TOKEN_COUNT];
    Object* obj;           /* I676c: the owning class object, for the live THEME redraw */
    struct aa_ctx* next;   /* I676c: every set-up context is in one list (aa_setup .. aa_cleanup) */
    int linked;
    struct TextFont* plain; /* I685w run 6: PLAIN backend tone font (diskfont), NULL = the object's MUI font */
    int plain_bold;         /* I685w run 6: PLAIN backend draws the title and heading tones in algorithmic bold */
} aa_ctx;

/* Backend, font, and the n colours in rgb[] as held pens. Call from MUIM_Setup after the super class. */
void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n);
/* Another font size on top of aa_setup (the title tone, base + 4); the bitmap tier and a failed open keep the base font. */
void aa_set_font_px(aa_ctx* c, int px);
/* I676c (live THEME): every set-up context drops its pens, copies the tokens in force (mui_theme_tokens), and its object redraws;
 * pens are re-obtained lazily by aa_pen at the next draw. Returns how many contexts were refreshed. */
uint32_t aa_retheme_all(void);
/* I685w run 6: the PLAIN backend (no ttengine or no Inter, rig 2) has no scalable font, so aa_set_font_px cannot grow a tone there.
 * This opens the bitmap disk font (mui_aa_screen_font: helvetica at the largest stock size not above px + 1) for the tone and
 * draws it bold when asked. No-op on the antialiased backends, for px <= 0 or when the open fails (the MUI font stays).
 * Call after aa_setup; aa_cleanup closes it. */
void aa_plain_font(aa_ctx* c, Object* obj, int px, int bold);
/* I685w run 6: the font the PLAIN backend draws in for this context (the tone font, else the object's MUI font). */
struct TextFont* aa_plain_textfont(const aa_ctx* c, Object* obj);
/* Releases every held pen and the font reference. Call from MUIM_Cleanup. */
void aa_cleanup(aa_ctx* c);
/* True when obj sits in an open window (between MUIM_Setup and MUIM_Cleanup). _win(obj) alone reads through the render
 * info, which is NULL before the first MUIM_Setup: that read returns whatever lies at address 0, and a redraw on that
 * garbage window crashed the Amiga (I684a, AACycle set before the window opened). Use this before every redraw from
 * OM_SET or an outside call. */
int aa_live(Object* obj);
/* I685w: every fill takes the 0xRRGGBB itself. On a truecolour screen (c->tc) the RGB reaches the screen unchanged through
 * p96RectFill or FillPixelArray; on a palette screen it goes through the nearest held pen. Classes never see a pen. */
/* Palette screens: hold the pen of rgb now, so a later draw cannot find the colour table full. A no-op on truecolour. */
void aa_hold(aa_ctx* c, uint32_t rgb);
LONG aa_nearest_pen(aa_ctx* c, uint32_t rgb);
/* Inclusive rectangle (x0, y0)-(x1, y1). */
void aa_rect(aa_ctx* c, struct RastPort* rp, int x0, int y0, int x1, int y1, uint32_t rgb);
/* Rows with the insets of mui_aa_round_inset; radius 0 is one rectangle. */
void aa_round_fill(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb);
/* aa_round_fill that leaves the hole rectangle (hx, hy, hw, hh) untouched: for a Group subclass that paints after its super call
 * around a child the super already drew. */
void aa_round_fill_holes(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, const int* holes, int n);
void aa_round_fill_hole(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, int hx, int hy, int hw, int hh);
/* Only the four corner wedges outside the rounded shape, in rgb: rounds a box whose square face its background already filled. */
void aa_round_corners(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb);
/* One pixel rounded outline, same inset maths. */
void aa_round_frame(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb);
/* Runs of equal nonzero slot in one row of n cells at (x, y); slot_rgb[slot] is the colour (glyph rows). */
void aa_runs(aa_ctx* c, struct RastPort* rp, int x, int y, const unsigned char* slot, int n, const uint32_t* slot_rgb);
/* Clear the box (x, y, w, h) of obj to its background: bg_rgb on truecolour, MUIM_DrawBackground otherwise. */
void aa_erase(aa_ctx* c, Object* obj, int x, int y, int w, int h);
/* Caption pixel size. */
int aa_text_width(aa_ctx* c, Object* obj, const char* str);
int aa_text_height(aa_ctx* c, Object* obj);
/* Draw str with its top-left at x,y. fg and bg are 0xRRGGBB; an unreadable pair (contrast gate) uses the DrawInfo text pen. */
void aa_text(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg, uint32_t bg);
/* Process wide: closes ttengine at the last class free. */
void aa_free(void);

#endif
