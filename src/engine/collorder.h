/* I994d1: the order of a collection or playlist page (design doc 2026-11-02-I994-play-context-design.md section 11.4).
 * Pure: builds "shown position -> stored row index" from per-row sort records. The engine owns the result; no UI sorts or
 * filters a page. */
#ifndef SELECTA_ENGINE_COLLORDER_H
#define SELECTA_ENGINE_COLLORDER_H

#include <selecta/engine.h>
#include <stdint.h>

typedef struct sel_coll_rec {
    const char* key;   /* song key; its file name is the title when `title` is empty */
    const char* title; /* catalogue title (songs.name), "" when unset */
    const char* artist;
    const char* album;
    int64_t added; /* songs.added, seconds */
    int64_t duration_ms;
} sel_coll_rec;

/* The title a row shows and sorts by: the catalogue title, else the file name of the key (what the desktop brief fallback
 * shows, desktop_ui.c pls_briefs_follow / path_base). Returns a pointer into the record. */
const char* sel_coll_title(const sel_coll_rec* r);

/* Writes into out[0..n) the stored indices of the rows that pass `filter` (case-insensitive substring of title, artist or
 * album; "" passes all), ordered by `sort` (SEL_COLL_SORT_*) and `descending`. Ties keep the stored order in BOTH directions.
 * Custom Order keeps stored order (descending reverses it). Returns the shown count. */
uint32_t sel_coll_order_build(const sel_coll_rec* rec, uint32_t n, uint32_t sort, uint32_t descending, const char* filter, uint32_t* out);

#endif
