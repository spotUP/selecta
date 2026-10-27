/* Copper tint of the 1-bit covers (MUI-P5-8, I510): the pure part. One exclusive pen is set per scanline to the average
 * colour of the matching cover row; the class turns the entries into a user copper list on the screen ViewPort. */
#ifndef MUI_COPPER_H
#define MUI_COPPER_H

#include <stdint.h>

typedef struct mui_copper_entry {
    int32_t y;     /* scanline relative to the screen top */
    uint16_t rgb4; /* OCS/AGA-safe 12-bit colour 0x0RGB the pen takes from this line on */
} mui_copper_entry;

/* Average colour of each of the h rows of a w x h cover (0xRRGGBB per row). */
void mui_copper_row_colors(const uint32_t* px, int32_t w, int32_t h, uint32_t* rows);
/* One big cover (window y `top`, h rows): an entry per row, consecutive equal 12-bit colours merged. Returns the count
 * (at most cap). */
uint32_t mui_copper_cover_list(const uint32_t* rows, int32_t h, int32_t top, mui_copper_entry* out, uint32_t cap);
/* Grid trial switch: on, tiles on one grid row share the line colour of their first tile's rows (`row_colors` of that
 * tile, tile height `th`) so one entry per scanline serves the whole row; off, the grid uses two MUI pens and adds
 * nothing. Returns the entries written. */
uint32_t mui_copper_grid_row_list(int grid_tint, const uint32_t* row_colors, int32_t th, int32_t top, mui_copper_entry* out,
                                  uint32_t cap);

#endif
