/* I669, I670: the Your Library kind filter, one model for every UI. The filter in force is an index 0..SEL_UIM_LFILTERS-1: 0 is
 * All, then one per kind chip (Playlists, Albums, Artists, Composers, Sources, Local Files). The desktop chips and the MUI
 * previous/next control both name, clamp, mask and read the list through these functions. Pure C99. */
#ifndef SELECTA_UIMODEL_LIBFILTER_H
#define SELECTA_UIMODEL_LIBFILTER_H

#include <selecta/engine.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_LFILTERS 7u /* All plus the six kind chips */

/* "All", "Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"; NULL past the count. */
const char* sel_uim_lfilter_label(uint32_t filter);
/* The SEL_LIB_ITEM_BIT mask the library call takes for `filter`; All and any index past the count give SEL_LIB_ITEM_ALL. */
uint32_t sel_uim_lfilter_mask(uint32_t filter);
/* One step of the previous (dir < 0) or next (dir > 0) control from `filter`, clamped: it stops at All and at the last chip. */
uint32_t sel_uim_lfilter_step(uint32_t filter, int dir);
/* The filter pass: the rows of the kinds `filter` names, read from the library store (the store filters, not a UI loop). Returns the
 * rows written to out[0..max). *total, when not NULL, is the row count of the whole filtered list. */
uint32_t sel_uim_lfilter_pass(sel_engine* e, uint32_t filter, sel_engine_library_item* out, uint32_t max, uint32_t* total);
/* I669: chips or the single label. The kind chips show side by side only when they fit the sidebar; otherwise (and always on the
 * bitmap tier) one label names the filter in force and the Previous and Next chevrons step it, and the label takes no click. Both UIs
 * measure their chip widths in their own units and ask here. total is the chips' summed width with gaps, band the width they may use. */
int32_t sel_uim_lfilter_chips_total(const int32_t* widths, uint32_t n, int32_t gap);
int sel_uim_lfilter_chips_single(int bitmap, int32_t total, int32_t band);
/* I1073: a chip row that scrolls sideways by whole chips (the Search page source chips). The last chip that may lead the row: the
 * smallest first index from which the chips first..n-1 fit band, so the right chevron stops once the last chip is in view. 0 when
 * the whole row fits. */
uint32_t sel_uim_lfilter_chips_first_max(const int32_t* widths, uint32_t n, int32_t gap, int32_t band);
/* I918: the visualizer chooser. A tab row with every full plugin label when the labels (label_w[0..n), gap between) fit avail,
 * else one "< label >" chooser. Returns 1 for tabs. Both UIs measure their labels in their own units and ask here. */
int sel_uim_viz_tabs_fit(const int32_t* label_w, uint32_t n, int32_t gap, int32_t avail);
/* I831: the label mode control is "< label >" (Spotify). With All in force the left button is Previous Filter; while any other filter
 * is in force it is Clear Filter (an x) and returns to All. Both UIs ask here for which one to show, its name, and what a press of the
 * left (dir < 0) or right (dir > 0) button gives; the right button always steps on (clamped). */
int sel_uim_lfilter_left_clears(uint32_t filter);
const char* sel_uim_lfilter_left_name(uint32_t filter);
uint32_t sel_uim_lfilter_single_step(uint32_t filter, int dir);
/* I1222d: the one wrap routine of every chip row (the sidebar chips, I1221, the Search page source chips, I1224, and the text sidebar).
 * Chips of widths w[0..n) with gap between them flow left to right in a band `band` wide; a chip that does not fit the rest of its row
 * (and is not first on it) starts the next. Units are the caller's (px, cells). sel_uim_chips_place gives chip k's x offset in the band
 * and its row (k >= n gives the spot after the last chip); sel_uim_chips_flow writes every chip's x and row and returns the row count
 * (0 when n is 0). The By You sub-row of Playlists stays a caller concern. */
void sel_uim_chips_place(const int32_t* w, uint32_t n, int32_t gap, int32_t band, uint32_t k, int32_t* x, int32_t* row);
int32_t sel_uim_chips_flow(const int32_t* w, uint32_t n, int32_t gap, int32_t band, int32_t* x, int32_t* row);
/* Passes run through sel_uim_lfilter_pass since start: the call-count sentinel the reachability tests read. */
uint32_t sel_uim_lfilter_passes(void);

#ifdef __cplusplus
}
#endif

#endif
