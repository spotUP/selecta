/* Builds a text atlas (src/gui/text/gui_text.h) from a TrueType blob with stb_truetype (DUI-P2-3). This is the one
 * place the desktop UI uses float: stb_truetype rasterizes in float. The result is plain integer tables, so
 * src/gui/text itself stays integer-only. */
#ifndef SEL_DESKTOP_ATLAS_BUILD_H
#define SEL_DESKTOP_ATLAS_BUILD_H

#include <selecta/gui/gui_text.h>

#include <stddef.h>

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
/* The UI scales (Q16) the atlases are baked for: the fit floor 0.625 (the 672x270 and 640x512 Amiga screens) and 1.0
 * (fit on a large screen and the bitmap tier, zoom 100). The 68k image has no TrueType fallback (I587), so a scale
 * missing here is a font miss on the Amiga; tests/dui/font_baked_unit.c fails when one of these is not baked. Zoom 75,
 * 125 and 150 and fit between 0.625 and 1.0 build at run time on the host only: baking them costs about 918 KB of
 * image and pushes vamos over -m 8192 (open, I587). */
#define DESK_FONT_BAKE_SCALES_Q16 40960, 65536
/* The tall-pixel (ydiv 2) Inter set (I526): SemiBold headlines only, at scale 1.0 (I545: px 24 is 12 rows, px 18 is 9).
 * Body text at ydiv 2 uses the Amiga bitmap fonts below. */
#define DESK_FONT_BAKE_TALL_PX 18, 24
#define DESK_FONT_BAKE_TALL_PX_N 2

/* Font families a baked atlas belongs to. 0 and 1 equal the old `semibold` flag, so existing callers stay valid. */
enum {
    DESK_FAMILY_INTER_REGULAR = 0,
    DESK_FAMILY_INTER_SEMIBOLD = 1,
    DESK_FAMILY_MINI4 = 2,
    DESK_FAMILY_MINI6 = 3,
    DESK_FAMILY_MINI8 = 4,
    DESK_FAMILY_N = 5
};

/* I526: an Amiga bitmap disk font (loadable .font program file, e.g. mini8/8) as an atlas at ydiv 2: glyph rows are
 * the font rows, layout units count twice per row. NULL on a malformed file. desk_amiga_font_ysize returns tf_YSize
 * (0 when the blob is not an Amiga font). Reader lives in amiga_font.c. */
sel_gui_atlas* desk_atlas_build_amiga(const unsigned char* data, size_t n);
int desk_amiga_font_ysize(const unsigned char* data, size_t n);

/* Baked atlases (I463): the 68020 tier has no floating point, so building one with stb_truetype on the first frame cost
 * 43.8 s on the Replay. tools/gui_fontbake.c runs desk_atlas_build on the host and writes the tables. */
typedef struct desk_baked_font {
    int32_t family; /* DESK_FAMILY_* */
    int32_t px;
    int32_t ydiv; /* 1 square pixels, 2 the 640x256 tall-pixel bake (I468) */
    sel_gui_atlas at; /* at.alpha is NULL in the table: the page is stored packed (I587), desk_atlas_baked decodes it */
    const uint8_t* packed;
    uint32_t packed_n;
} desk_baked_font;
extern const desk_baked_font sel_font_baked[];
extern const uint32_t sel_font_baked_n;
/* The baked atlas for a family and a pixel size, or NULL when that pair was not baked. The packed page (the icon packer,
 * sel_gui_pack_decode) is decoded on first use and kept, so the image holds the compressed page and RAM the pages in use. */
const sel_gui_atlas* desk_atlas_baked(int family, int pixel_size, int ydiv);

/* The embedded Inter blobs (generated at build time from third_party/fonts). */
#ifndef SEL_DESKTOP_NO_TTF /* the 68k image does not embed them (I587) */
extern const unsigned char sel_font_inter_regular[];
extern const unsigned char sel_font_inter_semibold[];
#endif

#endif
