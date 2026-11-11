/* Engine lines and texts of the Sources page, the sync offer and the search source (MUI-P8-8), lifted out of plugins/ui/desktop so
 * every UI sends the same lines. Pure C99. */
#ifndef SELECTA_UIMODEL_SOURCES_H
#define SELECTA_UIMODEL_SOURCES_H

#include <stddef.h>
#include <stdint.h>

#include <selecta/source.h>
#include <selecta/tags.h>

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
/* The flip of sel_uim_sources_off_line that toggles no source. */
#define SEL_UIM_SOURCES_NO_FLIP 0xFFFFFFFFu
/* I1308c: the sync offer's Never (desktop nagdlg_close action 0, I602): the sources in bitmask `ticked` off, every other source as
 * stored (no flip). 0 when nothing is ticked (Never then sends nothing) or the line does not fit. */
uint32_t sel_uim_sources_never_line(const sel_uim_source* s, uint32_t n, uint32_t ticked, char* out, uint32_t cap);
/* LIBRARY_SYNC <key> "<path>": the per-source Sync button and the offer's Sync button. */
uint32_t sel_uim_sync_line(const char* key, const char* path, char* out, uint32_t cap);
/* The offer's Not now: PREF_SET library.sync_notice_at <unix time>. */
uint32_t sel_uim_sync_notnow_line(int64_t now, char* out, uint32_t cap);
/* Status text of a source row: "Syncing" while pending, "Sync failed" after a failure, "Synced" once synced, else "Not synced". */
const char* sel_uim_sync_status(int pending, int failed, int64_t synced);

/* One SOURCES row as every UI holds it (I1197): the parse of the SEL_SRC_* tags of a LIBRARY_RESULT and the status text live here,
 * so the desktop, the MUI and any other UI read and word a row the same way. Plain bytes: a row may be memcmp'd for a change. */
typedef struct sel_uim_src_row {
    char key[32], kind[16], name[64], path[256];
    int enabled, searchable, syncable, due;
    uint32_t count;
    int64_t synced, snooze;
} sel_uim_src_row;
/* Fill `r` from the tags of one SOURCES row (SEL_SRC_KEY present); absent tags read as an enabled, unsynced, empty local row. */
void sel_uim_src_parse(const sel_tag* t, sel_uim_src_row* r);
int sel_uim_src_remote(const sel_uim_src_row* r);   /* kind "remote" */
int sel_uim_src_syncable(const sel_uim_src_row* r); /* a remote source the engine can sync: a search-only source has no Sync and no sync status */
/* "last synced N days ago" or "never synced" (synced = unix time, 0 never). */
uint32_t sel_uim_sync_age(int64_t synced, int64_t now, char* out, size_t cap);
/* Status text of a row: "Local, switched on", "Remote, switched off", "Remote, search only", "Remote, Syncing", "Remote, sync failed",
 * "Remote, last synced N days ago", "Remote, never synced". Returns the length. */
uint32_t sel_uim_src_status(const sel_uim_src_row* r, int pending, int failed, int64_t now, char* out, size_t cap);

/* The first-start sync offer (I527, I602, I1182), lifted out of the desktop so every UI decides and words it the same way. The engine
 * marks a SOURCES row `due`; the offer opens once per launch while one is due, lists the sources due at that moment (kept by key: the
 * rows are rebuilt by every SOURCES answer), and starts with every row ticked. Sync sends the ticked rows and snoozes the unticked
 * ones; Not now stores the global notice time (sel_uim_sync_notnow_line). */
typedef struct sel_uim_offer {
    char key[SEL_SOURCE_MAX][24];
    uint32_t n;   /* rows listed */
    uint32_t off; /* bit k set: row k is unticked */
    int done;     /* the offer was shown this launch */
} sel_uim_offer;
uint32_t sel_uim_offer_due(const sel_uim_src_row* r, uint32_t n); /* rows the engine marked due */
/* 1 when the offer should open now: not shown yet, nothing else blocks it (`blocked`: another dialog, a SOURCES answer pending, no frame yet), a source is due. */
int sel_uim_offer_wanted(const sel_uim_offer* o, int blocked, const sel_uim_src_row* r, uint32_t n);
void sel_uim_offer_open(sel_uim_offer* o, const sel_uim_src_row* r, uint32_t n); /* list the due rows, all ticked, mark shown */
void sel_uim_offer_toggle(sel_uim_offer* o, uint32_t k);
int sel_uim_offer_ticked(const sel_uim_offer* o, uint32_t k);
/* "N source(s) not synced for a while" */
uint32_t sel_uim_offer_title(const sel_uim_offer* o, char* out, size_t cap);
/* One row's text: "<name>, Syncing" while its sync runs, else "<name>, last synced N days ago" / "<name>, never synced". */
uint32_t sel_uim_offer_row_text(const sel_uim_src_row* r, int pending, int64_t now, char* out, size_t cap);
/* The rows now holding the listed keys, as bit masks over r[]; a key the last SOURCES answer no longer lists is skipped. */
void sel_uim_offer_masks(const sel_uim_offer* o, const sel_uim_src_row* r, uint32_t n, uint32_t* ticked, uint32_t* unticked);
/* PREF_SET library.sync_snoozed "key:days,...": days = now/86400 for the rows in `unticked`, the engine's own snooze for the others
 * (entries with days <= 0 are left out). Returns the line length, 0 when it does not fit. */
uint32_t sel_uim_offer_snooze_line(const sel_uim_src_row* r, uint32_t n, uint32_t unticked, int64_t now, char* out, uint32_t cap);

/* I1224: the search scope (the source key of one searchable, enabled source) of the source page named `page_key` (a source key or its
 * display name); NULL when no such source can be searched. The Search page opens on this scope from a source page, on All sources
 * ("" scope) from any other. The one place that decides the scope a page implies. */
const char* sel_uim_search_scope_of_source(const sel_uim_src_row* r, uint32_t n, const char* page_key);

/* Search source Cycle: Library, Web, Demozoo. Choosing one sends PREF_SET search.source "<name>"; the stored value selects the entry. */
#define SEL_UIM_SEARCH_SOURCES 3u
const char* sel_uim_search_source_name(uint32_t i);
uint32_t sel_uim_search_source_line(uint32_t i, char* out, uint32_t cap);
uint32_t sel_uim_search_source_find(const char* stored); /* index of the stored name, 0 when unknown */

#ifdef __cplusplus
}
#endif
#endif
