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
} aa_ctx;

/* Backend, font, and the n colours in rgb[] as held pens. Call from MUIM_Setup after the super class. */
void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n);
/* Releases every held pen and the font reference. Call from MUIM_Cleanup. */
void aa_cleanup(aa_ctx* c);
/* Held pen for an RGB, obtained on first use; -1 when the table is full. */
LONG aa_pen(aa_ctx* c, uint32_t rgb);
/* Rows of RectFill with the insets of mui_aa_round_inset; radius 0 is one RectFill. */
void aa_round_fill(struct RastPort* rp, int x, int y, int w, int h, int radius, LONG pen);
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
