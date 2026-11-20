/* I1473g3 (design 2026-11-20-i1473g-mui-cover-grid.md section 5, D2): the cover loader of the MUI grid. Tiles are drawn from a
 * tile cache at the grid edge; a miss reads the shared desktop thumbnail row "thumbf144" of the album (art_id = album id), or
 * decodes the stored cover, fits it into 144 x 144 and writes the thumbnail back so the Mac and the grid share it. */
#ifndef MUI_GRID_ART_H
#define MUI_GRID_ART_H

#include "mui_canvas.h"

#include <selecta/ui.h>

#include <stdint.h>

#define MUI_GRID_THUMB_EDGE 144 /* DESK_ART_TILE_EDGE: the stored thumbnail size, kind "thumbf144" */

/* I1473g8b6 (D-F1): the one loader serves both styles. Cover Flow loads at the shared thumbnail edge (side covers sharp, the front cover
 * is upscaled by the draw) in 12 slots (9 visible + 3 ahead); the grid keeps its own edge and 24 slots. */
int32_t mui_flow_art_edge(int32_t grid_edge, int flow);
uint8_t mui_flow_art_slots(uint8_t grid_slots, int flow);
/* Decodes the stored cover (still `frame`, 0 = cover) of `album` to malloc'd 0xAARRGGBB words (w x h), NULL when it has none.
 * The plugin passes mui_cover_album_image; the decoder (stb_image) lives in the plugin, so the host test passes a fake. */
typedef uint32_t* (*mui_grid_decode_fn)(const sel_ui_art* art, int64_t album, uint32_t frame, int32_t* w, int32_t* h);

typedef struct mui_grid_art mui_grid_art;

/* `slots` tile slots of `edge` x `edge` words. NULL when art or decode is NULL, edge is out of 1..144, or out of memory. */
mui_grid_art* mui_grid_art_open(const sel_ui_art* art, mui_grid_decode_fn decode, int32_t edge, uint8_t slots);
void mui_grid_art_close(mui_grid_art* g);
/* A mui_art_fn (ctx = the loader): cached tile words of the album, NULL on a miss. Never loads. */
const uint32_t* mui_grid_art_get(void* g, int64_t art_id);
/* One UI tick: serves at most one miss, the first tile whose cover is neither cached nor known bad. Returns the art_id loaded
 * (0 = nothing to do, or the album has no usable cover; the tile keeps the platform cover). */
int64_t mui_grid_art_tick(mui_grid_art* g, const mui_tile* tiles, uint32_t n);
/* Full decodes done so far: the sentinel that a thumbnail was not enough. */
uint32_t mui_grid_art_decodes(const mui_grid_art* g);

#endif
