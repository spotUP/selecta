/* The `asma` source sync (row I413; online-sources plan section 2; design doc 9.1): ASMA, the Atari SAP Music
 * Archive (https://asma.atari.org). Its web player loads one file, /asmadb/asmadb.js, a JavaScript object whose
 * "fileInfos" list has, per song, its path below /asma/, title, author ("Real Name (Handle)") and size. The sync
 * streams that file (2.2 MB on the real site) without holding it in memory and writes one catalogue row per song
 * through the same SCAN_BATCH request as the other sources. Format names follow bbmp: "Slight Atari Player" for
 * .sap, "TIATracker" for .ttt. A path is relative to the /asma/ root, the song key is `asma:<path>`. */
#ifndef SELECTA_LIBRARY_SOURCES_ASMA_SYNC_H
#define SELECTA_LIBRARY_SOURCES_ASMA_SYNC_H

#include "local_source.h"

#include <stdio.h>

#define SEL_ASMA_LIST "asmadb.js"
#define SEL_ASMA_LIST_DIR "/asmadb/"
#define SEL_ASMA_ROOT "asma/" /* where the songs live on the site: <site>/asma/<path> */

#define SEL_ASMA_PATH_MAX 512u
#define SEL_ASMA_TEXT_MAX 192u

typedef struct sel_asma_entry {
    char path[SEL_ASMA_PATH_MAX];
    char title[SEL_ASMA_TEXT_MAX];
    char author[SEL_ASMA_TEXT_MAX];
    int64_t size; /* -1 when the list gives none */
} sel_asma_entry;

typedef struct sel_asma_sync_stats {
    uint32_t entries;      /* fileInfos objects read from the list */
    uint32_t listing_rows; /* rows written to the catalogue */
    uint32_t skipped_rows; /* entries with an unsafe or missing path */
    uint32_t batches;      /* SEL_LIB_REQ_SCAN_BATCH requests sent */
} sel_asma_sync_stats;

/* Calls `fn` once per object of the "fileInfos" array of `f`; `fn` returns 0 to stop. Long strings are cut to the
 * entry field sizes. Returns the objects read, or -1 when the list has no "fileInfos" array. */
int32_t sel_asma_read_list(FILE* f, int (*fn)(void* ctx, const sel_asma_entry* e), void* ctx);

/* "Dominique Berger (StaxX)" -> "StaxX"; "<?> (Aki)" -> "Aki"; "Jakub Husak" as it is; "<?>" or "" ->
 * "- unknown" (Modland's word); "A (x) & B (y)" -> "x & y" (bbmp asma.py artist_of). 0 when `out` is too small. */
int sel_asma_artist(const char* author, char* out, size_t n);

/* Reads <dir>/asmadb.js into the catalogue. Returns 0 when the file cannot be read or has no "fileInfos", memory
 * runs out or the DB rejects a batch. */
int sel_asma_sync(const sel_local_scan_config* cfg, const char* dir, sel_asma_sync_stats* stats);

#endif
