/* Picasso96 pixel push shared by the desktop RTG backend and the MUI canvas class (one copy; the Replay has no
 * cybergraphics.library). Header-only, Amiga builds only: include <inline/macros.h>, <graphics/rastport.h> first via the
 * caller's proto headers. Picasso96.h: struct RenderInfo, RGBFB_*. */
#ifndef SEL_SHARED_AMIGA_P96_WRITE_H
#define SEL_SHARED_AMIGA_P96_WRITE_H

#include <exec/types.h>
#include <graphics/rastport.h>
#include <inline/macros.h>

struct p96_render_info {
    APTR memory;
    WORD bytes_per_row;
    WORD pad;
    ULONG rgb_format;
};
#define P96_FMT_CLUT 1u      /* RGBFB_CLUT */
#define P96_FMT_A8R8G8B8 6u  /* RGBFB_A8R8G8B8 */

/* p96WritePixelArray (LVO -102): write a w x h block of ri starting at (sx, sy) to rp at (dx, dy). */
static inline void p96_write_pixel_array(struct Library* p96, struct p96_render_info* ri, LONG sx, LONG sy,
                                         struct RastPort* rp, LONG dx, LONG dy, LONG w, LONG h) {
    LP8NR(0x66, p96WritePixelArray, struct p96_render_info*, ri, a0, UWORD, sx, d0, UWORD, sy, d1, struct RastPort*, rp, a1,
          UWORD, dx, d2, UWORD, dy, d3, UWORD, w, d4, UWORD, h, d5, , p96);
}

#endif
