/* I526: the font role table of the bitmap tier (AGA 640x256, ydiv 2, scale 1.0). One place maps a size token and a
 * weight to a baked atlas: Amiga bitmap fonts for text, Inter SemiBold for the two headline sizes. */
#ifndef SELECTA_DESKTOP_FONT_ROLES_H
#define SELECTA_DESKTOP_FONT_ROLES_H

#include "atlas_build.h"

/* The baked atlas for (semibold, size token). box_h > 0 is the box height in layout units: while the line is taller
 * than the box the next smaller font is taken (headline 32, headline 22, mini8, then the 6-row fonts). NULL when
 * nothing is baked for the token. family and px describe the atlas chosen (either may be NULL). */
const sel_gui_atlas* desk_font_role_atlas(int semibold, int size, int32_t box_h, int* family, int* px);

#endif
