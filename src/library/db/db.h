/* Library database and worker (plan 2026-10-25-m3-library.md Phase 1; D-M3-1,
 * D-M3-2; design doc section 9.2). One worker owns the only SQLite
 * connection of a library; callers talk to it through a request and a result
 * SPSC queue. SQLite runs on a memsys5 heap allocated once at open and sized
 * from sel_machine (rule 4: no allocation after start). Every entry point
 * takes the library handle, which stands for the account (F52): no global
 * library. On the Amiga build requests run on the caller's task until the
 * library subtask lands (Phase 12). */
#ifndef SELECTA_LIBRARY_DB_H
#define SELECTA_LIBRARY_DB_H

#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/tags.h>

#include <stdint.h>

typedef struct sel_library sel_library;

/* Request codes (sel_message.id of a SEL_MSG_CMD on the request queue). */
enum {
    SEL_LIB_REQ_STATS = 1, /* counts plus an FTS5 self-check in a rolled-back savepoint */
    SEL_LIB_REQ_STOP = 2,
    SEL_LIB_REQ_SCAN_BATCH = 3 /* payload: const sel_scan_batch*; writes files and songs rows (I158) */
};

/* One catalogue row from a source (design doc 9.2 `files` plus the `songs` columns a scan knows).
 * Text is NUL-terminated, "" = unknown. */
typedef struct sel_scan_row {
    const char* source; /* "local" */
    const char* path;
    const char* format;
    const char* artist;
    const char* name;
    const char* md5;
    int64_t size;
    int32_t duration_ms; /* 0 = unknown */
    int32_t year;        /* 0 = unknown */
} sel_scan_row;

typedef struct sel_scan_batch {
    const sel_scan_row* rows;
    uint32_t count;
    int64_t now; /* epoch seconds stored in songs.added */
} sel_scan_batch;

/* Reply to SEL_LIB_REQ_SCAN_BATCH: INT [0] rows written, [1] SQLite result code (0 = ok). */

/* Reply to SEL_LIB_REQ_STATS (sel_message.id = the request code, kind SEL_MSG_EVT): args are INT
 * [0] schema_version, [1] fts5 compiled in, [2] fts_hits of the self-check, [3] songs rows,
 * [4] requests served so far, [5] SQLite result code of the last statement (0 = ok). */
enum {
    SEL_LIB_STAT_SCHEMA = 0,
    SEL_LIB_STAT_FTS5,
    SEL_LIB_STAT_FTS_HITS,
    SEL_LIB_STAT_SONGS,
    SEL_LIB_STAT_SERVED,
    SEL_LIB_STAT_RC
};

/* Schema version this build writes (PRAGMA user_version after migration). */
#define SEL_LIB_SCHEMA_VERSION 1

/* Heap bytes for SQLite on a machine: the scaling table of D-M3-2 over fast + chip RAM
 * (<= 4 MiB: 512 KiB, <= 16 MiB: 2 MiB, <= 128 MiB: 8 MiB, above: 32 MiB). */
uint32_t sel_library_heap_bytes(const sel_tag* machine);

/* Opens a library for `account` at `path` (":memory:" for a throwaway DB), allocates the heap
 * once, starts the worker, runs the schema and migrations. NULL on failure. */
sel_library* sel_library_open(const sel_host* host, const sel_tag* machine, uint32_t account, const char* path);
void sel_library_close(sel_library* lib);

/* Sends a request and waits for its reply. 1 on success. */
int sel_library_call(sel_library* lib, uint32_t req, sel_message* reply);
/* Same with a payload the worker reads while the caller waits (carried in args[0].v.s). */
int sel_library_request(sel_library* lib, uint32_t req, const void* payload, sel_message* reply);

uint32_t sel_library_account(const sel_library* lib);
uint32_t sel_library_heap_size(const sel_library* lib);
/* Heap allocations made so far by this library (1 after a successful open). */
uint32_t sel_library_heap_allocs(const sel_library* lib);
const char* sel_library_sqlite_version(void);

#endif
