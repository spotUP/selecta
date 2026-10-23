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
    uint32_t archive_lzx;     /* LZX archives opened (I159a) */
    uint32_t depack_pp;       /* PowerPacker files depacked and scanned (I159a) */
    uint32_t rescanned_dirs;  /* directories rescanned by sel_source_rescan (I160) */
} sel_local_scan_stats;

/* Scans `root` recursively. Returns 0 when the root cannot be listed, memory runs out or the DB
 * rejects a batch; stats are filled either way. All scratch memory is allocated once up front. */
int sel_local_scan(const sel_local_scan_config* cfg, const char* root, sel_local_scan_stats* stats);

/* I160 (design doc F16): rescans exactly the directories in `paths` (each one flat, subdirectories are
 * not entered) into the catalogue with the same scan code as sel_local_scan. Sets stats->rescanned_dirs
 * to the number of directories listed. Returns 0 when a directory cannot be listed, memory runs out or
 * the DB rejects a batch. The watcher (src/library/watch.h) feeds it on the desktop; the Amiga timer
 * feeds it through sel_local_poll_changed. */
int sel_source_rescan(const sel_local_scan_config* cfg, const char* const* paths, uint32_t count,
                      sel_local_scan_stats* stats);

/* mtime snapshot of every directory under a root (at most SEL_TRACK_DIRS), the no-watcher path. */
#define SEL_TRACK_DIRS 64u
typedef struct sel_local_tracker sel_local_tracker;

sel_local_tracker* sel_local_track(const sel_local_scan_config* cfg, const char* root);

/* Re-reads the directory mtimes, rescans (flat) every directory whose mtime moved or that is new,
 * and updates the snapshot. Returns the number of directories rescanned; stats as sel_source_rescan. */
uint32_t sel_local_poll_changed(const sel_local_scan_config* cfg, sel_local_tracker* t, sel_local_scan_stats* stats);

void sel_local_untrack(const sel_local_scan_config* cfg, sel_local_tracker* t);

#endif /* SELECTA_LIBRARY_SOURCES_LOCAL_SOURCE_H */
