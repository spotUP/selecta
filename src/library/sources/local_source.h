/* The `local` source scan (plan Phase 4, I158; design doc 9.1, 9.2): walks a directory tree with
 * sel_browse, probes every file with the registered replayers, and writes one `files` and one
 * `songs` row per recognised module through the library worker (SEL_LIB_REQ_SCAN_BATCH). */
#ifndef SELECTA_LIBRARY_SOURCES_LOCAL_SOURCE_H
#define SELECTA_LIBRARY_SOURCES_LOCAL_SOURCE_H

#include "db.h"

#include <selecta/host.h>
#include <selecta/tags.h>
#include <stdint.h>

typedef struct sel_local_scan_config {
    const sel_host* host;
    sel_library* lib;
    /* Plugin enumeration, same pair the engine takes (the library does not link the registry). */
    uint32_t (*registry_count)(void);
    const sel_tag* (*registry_get)(uint32_t index);
    int64_t now; /* epoch seconds for songs.added */
} sel_local_scan_config;

typedef struct sel_local_scan_stats {
    uint32_t scanned_files; /* files recognised by a replayer and written to the DB */
    uint32_t skipped_files; /* files no replayer claimed, unreadable or over the size cap */
    uint32_t scan_batches;  /* SEL_LIB_REQ_SCAN_BATCH requests sent */
    uint32_t probed;        /* replayer probe calls made */
    uint32_t dirs;          /* directories listed */
    uint32_t archives;        /* archive files opened as folders (I159) */
    uint32_t archive_members; /* archive members recognised and queued */
} sel_local_scan_stats;

/* Scans `root` recursively. Returns 0 when the root cannot be listed, memory runs out or the DB
 * rejects a batch; stats are filled either way. All scratch memory is allocated once up front. */
int sel_local_scan(const sel_local_scan_config* cfg, const char* root, sel_local_scan_stats* stats);

#endif /* SELECTA_LIBRARY_SOURCES_LOCAL_SOURCE_H */
