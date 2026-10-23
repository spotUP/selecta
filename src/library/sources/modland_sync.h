/* The `modland` source sync (plan Phase 4, I161; design doc 9.1): reads the allmods_md5 listing into
 * the catalogue and applies the daily changelogs on top (incremental update). Offline by design: the
 * listing and the changelogs are files in one directory (the fetch layer puts them there; the
 * canonical URL of a row comes from sel_source_modland()->url). */
#ifndef SELECTA_LIBRARY_SOURCES_MODLAND_SYNC_H
#define SELECTA_LIBRARY_SOURCES_MODLAND_SYNC_H

#include "local_source.h"

#define SEL_MODLAND_LISTING "allmods_md5.txt"    /* `<md5> <path>` per line (or `<size>\t<path>`) */
#define SEL_MODLAND_CHANGELOG_PREFIX "changelog_" /* changelog_YYYY-MM-DD.txt: added:/moved:+to:/deleted:/updated: */

typedef struct sel_modland_sync_stats {
    uint32_t listing_rows;  /* listing lines written to the catalogue */
    uint32_t skipped_rows;  /* listing lines without format/artist/file structure */
    uint32_t batches;       /* SEL_LIB_REQ_SCAN_BATCH requests sent */
    uint32_t changelogs;    /* changelog files applied */
    uint32_t cl_added;      /* rows inserted by `added:` */
    uint32_t cl_removed;    /* rows removed by `deleted:` and the old side of `moved:` */
    uint32_t cl_moved;      /* `moved:` entries applied */
    uint32_t cl_updated;    /* `updated:` entries seen (rows keep their data until the next listing) */
    uint32_t new_this_week; /* `added:` entries in changelogs dated within 7 days up to `now` */
} sel_modland_sync_stats;

/* Applies <dir>/allmods_md5.txt, then every <dir>/changelog_*.txt in name (date) order. `now` is epoch
 * seconds (the home view's clock; tests fix it). Returns 0 when the listing cannot be read, memory runs
 * out or the DB rejects a batch. */
int sel_modland_sync(const sel_local_scan_config* cfg, const char* dir, int64_t now, sel_modland_sync_stats* stats);

#endif
