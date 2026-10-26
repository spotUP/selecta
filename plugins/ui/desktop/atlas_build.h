/* Builds a text atlas (src/gui/text/gui_text.h) from a TrueType blob with stb_truetype (DUI-P2-3). This is the one
 * place the desktop UI uses float: stb_truetype rasterizes in float. The result is plain integer tables, so
 * src/gui/text itself stays integer-only. */
#ifndef SEL_DESKTOP_ATLAS_BUILD_H
#define SEL_DESKTOP_ATLAS_BUILD_H

#include <selecta/gui/gui_text.h>

/* Code points the atlas holds: Basic Latin, Latin-1 Supplement and Latin Extended-A, and the general punctuation
 * the player texts use. Returns NULL on a bad font or no memory. */
sel_gui_atlas* desk_atlas_build(const unsigned char* ttf, int pixel_size);
/* I468: the atlas for pixels `ydiv` times taller than wide (PAL hires 640x256): glyph bitmaps hold 1/ydiv of the rows,
 * while ascent, line height and the bitmap offsets stay in the square-pixel layout space the text code works in. */
sel_gui_atlas* desk_atlas_build_tall(const unsigned char* ttf, int pixel_size, int ydiv);
/* Frees a built atlas; a baked one (static data) is left alone. */
void desk_atlas_free(const sel_gui_atlas* at);

/* The text size tokens as pixel sizes (plan 2.6); each gets one atlas per weight. Shared by desktop_ui.c and the
 * host tool tools/gui_fontbake.c so the baked set cannot drift from what the UI asks for. */
#define DESK_FONT_SIZE_N 10
#define DESK_FONT_SIZES 11, 12, 13, 14, 15, 16, 18, 22, 24, 32
/* The UI scales (Q16) the atlases are baked for: the fit floor 0.625 (the 672x270 and 640x512 Amiga screens) and 1.0.
 * Other scales build at run time, as before. */
#define DESK_FONT_BAKE_SCALES_Q16 40960, 65536
/* The scale a tall-pixel (ydiv 2) set is also baked for: the AGA 640x256 screen lays out at the fit floor. */
#define DESK_FONT_BAKE_TALL_SCALE_Q16 40960

/* Baked atlases (I463): the 68020 tier has no floating point, so building one with stb_truetype on the first frame cost
 * 43.8 s on the Replay. tools/gui_fontbake.c runs desk_atlas_build on the host and writes the tables. */
typedef struct desk_baked_font {
    int32_t semibold;
    int32_t px;
    int32_t ydiv; /* 1 square pixels, 2 the 640x256 tall-pixel bake (I468) */
    sel_gui_atlas at;
} desk_baked_font;
extern const desk_baked_font sel_font_baked[];
extern const uint32_t sel_font_baked_n;
/* The baked atlas for a weight and a pixel size, or NULL when that pair was not baked. */
const sel_gui_atlas* desk_atlas_baked(int semibold, int pixel_size, int ydiv);

/* The embedded Inter blobs (generated at build time from third_party/fonts). */
extern const unsigned char sel_font_inter_regular[];
extern const unsigned char sel_font_inter_semibold[];

#endif
