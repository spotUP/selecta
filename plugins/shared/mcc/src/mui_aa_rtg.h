/* I685w: the two truecolour fill calls of the MCC draw path, header-only, Amiga builds only. p96RectFill (Picasso96API.library) is
 * the first choice, FillPixelArray (cybergraphics.library) the fallback. Both take the RastPort, so the layer and the MUI clip
 * region apply. Kept inside the MCC tree: the class package builds without the Selecta source tree (mcc-include-check). */
#ifndef MUI_AA_RTG_H
#define MUI_AA_RTG_H

#include <exec/types.h>
#include <graphics/rastport.h>
#include <inline/macros.h>

/* p96RectFill (LVO -126): fill the inclusive rectangle (x0, y0)-(x1, y1) of rp with rgb (0x00RRGGBB) on a 15+ bit screen. */
static inline void aa_p96_rect_fill(struct Library* p96, struct RastPort* rp, LONG x0, LONG y0, LONG x1, LONG y1, ULONG rgb) {
    LP6NR(0x7E, p96RectFill, struct RastPort*, rp, a1, UWORD, x0, d0, UWORD, y0, d1, UWORD, x1, d2, UWORD, y1, d3, ULONG, rgb, d4, , p96);
}

/* FillPixelArray (LVO -150): fill the w x h block at (x, y) of rp with argb (0xAARRGGBB, alpha ignored). */
static inline void aa_cgx_fill_pixel_array(struct Library* cgx, struct RastPort* rp, LONG x, LONG y, LONG w, LONG h, ULONG argb) {
    LP6NR(0x96, FillPixelArray, struct RastPort*, rp, a1, UWORD, x, d0, UWORD, y, d1, UWORD, w, d2, UWORD, h, d3, ULONG, argb, d4, , cgx);
}

#endif
