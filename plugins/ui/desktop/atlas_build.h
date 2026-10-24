/* Builds a text atlas (src/gui/text/gui_text.h) from a TrueType blob with stb_truetype (DUI-P2-3). This is the one
 * place the desktop UI uses float: stb_truetype rasterizes in float. The result is plain integer tables, so
 * src/gui/text itself stays integer-only. */
#ifndef SEL_DESKTOP_ATLAS_BUILD_H
#define SEL_DESKTOP_ATLAS_BUILD_H

#include <selecta/gui/gui_text.h>

/* Code points the atlas holds: Basic Latin, Latin-1 Supplement and Latin Extended-A, and the general punctuation
 * the player texts use. Returns NULL on a bad font or no memory. */
sel_gui_atlas* desk_atlas_build(const unsigned char* ttf, int pixel_size);
void desk_atlas_free(sel_gui_atlas* at);

/* The embedded Inter blobs (generated at build time from third_party/fonts). */
extern const unsigned char sel_font_inter_regular[];
extern const unsigned char sel_font_inter_semibold[];

#endif
