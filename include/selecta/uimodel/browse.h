/* Library modes, queue scopes and tile size steps (lifted out of plugins/ui/desktop for MUI-P8-3, P8-4, P8-12). One model for
 * every UI: the names a mode tab or Cycle shows, and the engine lines each choice sends. Pure C99. */
#ifndef SELECTA_UIMODEL_BROWSE_H
#define SELECTA_UIMODEL_BROWSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_LIB_MODES 6u
#define SEL_UIM_LIB_LINES 5u
#define SEL_UIM_LIB_LINE_MAX 40u
#define SEL_UIM_LIB_SOURCES 5u /* the Sources mode index */

/* Mode 0 Composers, 1 Tree, 2 Columns, 3 Files, 4 History, 5 Sources. NULL past the count. */
const char* sel_uim_libmode_name(uint32_t mode);
/* The engine lines entering `mode`, the Library window being `window_rows` high. Returns the line count. */
uint32_t sel_uim_libmode_lines(uint32_t mode, uint32_t window_rows, char out[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX]);

#define SEL_UIM_QUEUE_SCOPES 3u
/* Scope 0 library, 1 album, 2 playlist: the engine word, and the menu label ("Scope: Library"). NULL past the count. */
const char* sel_uim_scope_name(uint32_t i);
const char* sel_uim_scope_label(uint32_t i);
/* "SCOPE <name>" into out; returns the length, 0 for an index past the count. */
uint32_t sel_uim_scope_line(uint32_t i, char* out, uint32_t cap);

#define SEL_UIM_TILE_STEPS 5u
/* The grid and home shelf tile size steps in logical pixels (96..208); 0 past the count. */
int32_t sel_uim_tile_step(uint32_t i);

/* I611: the Your Library sort choices and view sizes, one model for every UI. */
#define SEL_UIM_LSORTS 4u
enum { SEL_UIM_LSORT_RECENTS = 0, SEL_UIM_LSORT_ADDED = 1, SEL_UIM_LSORT_ALPHA = 2, SEL_UIM_LSORT_CREATOR = 3 };
const char* sel_uim_lsort_label(uint32_t i); /* "Recents", "Recently added", "Alphabetical", "Creator"; NULL past the count */
#define SEL_UIM_LVIEWS 4u
enum { SEL_UIM_LVIEW_COMPACT = 0, SEL_UIM_LVIEW_LIST = 1, SEL_UIM_LVIEW_SMALL = 2, SEL_UIM_LVIEW_LARGE = 3 };
const char* sel_uim_lview_label(uint32_t i); /* "Compact list", "List", "Small grid", "Large grid"; NULL past the count */
/* Orders two library entries under sort `mode`: <0 a first, >0 b first, 0 equal. Recents keeps the engine order (0), Recently added
 * puts the larger `added` first, Alphabetical and Creator compare case-insensitively (Creator then name). */
int32_t sel_uim_lsort_cmp(uint32_t mode, const char* a_name, uint32_t a_added, const char* a_creator, const char* b_name,
                          uint32_t b_added, const char* b_creator);
/* I678c: the kind-aware order of Your Library rows (design section 5). Recents puts the newer of last_played and opened first,
 * Recently added the larger `added`, Alphabetical and Creator compare names case-insensitively with a leading "The " ignored for
 * artists (D4); Creator compares `creator` first. `kind` is a SEL_LIB_ITEM_* value. */
typedef struct sel_uim_lsort_key {
    uint32_t kind;
    const char* name;
    const char* creator;
    uint32_t added, last_played, opened;
} sel_uim_lsort_key;
int32_t sel_uim_lsort_item_cmp(uint32_t mode, const sel_uim_lsort_key* a, const sel_uim_lsort_key* b);
/* 1 when `name` contains `filter` ignoring case (an empty filter matches everything). */
int32_t sel_uim_lfilter_match(const char* name, const char* filter);

/* I678a: the subtitle of a Your Library row (design 2026-10-29-desktop-library-architecture.md section 2), one formatter for every UI:
 * the pinned kinds (Liked, Recent, Local) "Playlist • N songs", Composer and Source "Kind • N songs" (singular "1 song"), Playlist the owner (`sub`, "You"), Album
 * "Album • <artist>" (just "Album" without one), Artist "Artist". `kind` is a SEL_LIB_ITEM_* value.
 * Writes at most cap bytes with a NUL; returns the length that would fit without a cap (snprintf style). */
uint32_t sel_uim_library_item_subtitle(uint32_t kind, const char* sub, uint32_t songs, char* out, uint32_t cap);
/* I678d: the kind of a row as one word ("Playlist", "Album", "Artist", "Composer", "Source"); the pinned kinds are "Playlist". */
const char* sel_uim_library_item_type(uint32_t kind);
/* I678k: the one-letter kind mark of the bitmap tier ("P" playlist, "A" album, "R" artist, "C" composer, "S" source; "*" for a pinned row). */
const char* sel_uim_library_item_mark(uint32_t kind, int pinned);
/* I678d: "5 minutes ago", "2 days ago" for the Date Added and Played columns; "-" when `then` is 0 or not an epoch time. */
uint32_t sel_uim_relative_time(int64_t now, uint32_t then, char* out, uint32_t cap);

/* I793: the Your Library sidebar texts and chips, one table for the desktop UI (side_lib_draw) and the MUI sidebar
 * (map/mui_sidebar.c). The chips are the kinds a library lists in Spotify's order (I610); each hands the library call a kind mask
 * (I678b). */
#define SEL_UIM_SIDE_CHIPS 6u
/* I1166b: the header of column i of the expanded Your Library table: "Title", "Type", "Date Added", "Played"; NULL past 3. */
const char* sel_uim_side_col_name(uint32_t i);
const char* sel_uim_side_chip_name(uint32_t i); /* "Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"; NULL past the count */
uint32_t sel_uim_side_chip_mask(uint32_t i);    /* the SEL_LIB_ITEM_BIT mask of chip i, 0 past the count */
typedef enum sel_uim_side_text {
    SEL_UIM_SIDE_TITLE = 0,  /* "Your Library" */
    SEL_UIM_SIDE_CREATE,     /* the plus button: "Create Playlist" */
    SEL_UIM_SIDE_EXPAND,     /* "Expand Library" */
    SEL_UIM_SIDE_COLLAPSE,   /* "Collapse Library" */
    SEL_UIM_SIDE_CHIP_ALL,   /* "All" */
    SEL_UIM_SIDE_CHIP_BYYOU, /* the second-level chip of Playlists */
    SEL_UIM_SIDE_EMPTY_KIND, /* a chip of a kind without rows */
    SEL_UIM_SIDE_EMPTY_NONE, /* the bitmap tier with no playlist */
    SEL_UIM_SIDE_CARD_TITLE,
    SEL_UIM_SIDE_CARD_BODY,
    SEL_UIM_SIDE_CARD_BUTTON,
    SEL_UIM_SIDE_TEXTS
} sel_uim_side_text;
const char* sel_uim_side_text_get(sel_uim_side_text id);
/* The empty state under the list: `chip` is 0 for All, 1..6 for a chip; `n` the rows listed; `bitmap` the bitmap tier. NULL when none is shown. */
const char* sel_uim_side_empty(uint32_t chip, uint32_t n, int bitmap);
/* I627, I678d: whether the "Create your first playlist" card shows: chip All or Playlists, no filter, no playlist row in the list, not the bitmap tier. */
int sel_uim_side_card_shown(uint32_t chip, int filtered, uint32_t playlist_rows, int bitmap);

#ifdef __cplusplus
}
#endif

#endif
