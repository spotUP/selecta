/* Engine lines and texts of the Sources page, the sync offer and the search source (MUI-P8-8), lifted out of plugins/ui/desktop so
 * every UI sends the same lines. Pure C99. */
#ifndef SELECTA_UIMODEL_SOURCES_H
#define SELECTA_UIMODEL_SOURCES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sel_uim_source {
    const char* key;  /* the id library.sources_off lists */
    const char* path; /* the home LIBRARY_SYNC is sent with */
    int enabled;
} sel_uim_source;

/* PREF_SET library.sources_off "<csv>" with source `flip` toggled (flip >= n: none) and the sources in bitmask `force_off` off.
 * Returns the line length, 0 when it does not fit. */
uint32_t sel_uim_sources_off_line(const sel_uim_source* s, uint32_t n, uint32_t flip, uint32_t force_off, char* out, uint32_t cap);
/* LIBRARY_SYNC <key> "<path>": the per-source Sync button and the offer's Sync button. */
uint32_t sel_uim_sync_line(const char* key, const char* path, char* out, uint32_t cap);
/* The offer's Not now: PREF_SET library.sync_notice_at <unix time>. */
uint32_t sel_uim_sync_notnow_line(int64_t now, char* out, uint32_t cap);
/* Status text of a source row: "Syncing" while pending, "Sync failed" after a failure, "Synced" once synced, else "Not synced". */
const char* sel_uim_sync_status(int pending, int failed, int64_t synced);

/* Search source Cycle: Library, Web, Demozoo. Choosing one sends PREF_SET search.source "<name>"; the stored value selects the entry. */
#define SEL_UIM_SEARCH_SOURCES 3u
const char* sel_uim_search_source_name(uint32_t i);
uint32_t sel_uim_search_source_line(uint32_t i, char* out, uint32_t cap);
uint32_t sel_uim_search_source_find(const char* stored); /* index of the stored name, 0 when unknown */

#ifdef __cplusplus
}
#endif
#endif
