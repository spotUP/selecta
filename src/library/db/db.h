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
    SEL_LIB_REQ_SEARCH = 3,    /* run a query text; keys come back in sel_lib_job.out (I149) */
    SEL_LIB_REQ_SAVE_VIEW = 4, /* store text2 as the saved query `text` (I150) */
    SEL_LIB_REQ_IMPORT = 5,    /* import a TSV catalogue held in `text` (test and CLI seeding) */
    SEL_LIB_REQ_SCAN_BATCH = 6, /* payload: const sel_scan_batch*; writes files and songs rows (I158) */
    SEL_LIB_REQ_REMOVE = 7,     /* payload: const sel_remove_batch*; deletes files and songs rows (I161) */
    SEL_LIB_REQ_ANNOTATE = 8    /* payload: const sel_annotate_batch*; lengths and notes onto existing songs (I162) */
};

/* The data of a SEARCH, SAVE_VIEW or IMPORT request. The caller fills the inputs, passes the job to
 * sel_library_run and blocks until the worker answers, so the pointers stay valid. No allocation:
 * `out` is a caller buffer of NUL-terminated keys. */
typedef struct sel_lib_job {
    const char* text;  /* SEARCH: query text; SAVE_VIEW: view name; IMPORT: catalogue TSV */
    const char* text2; /* SAVE_VIEW: the query text */
    long long now;     /* SEARCH: seconds since the epoch for relative times */
    char* out;         /* SEARCH: keys, each NUL terminated */
    uint32_t out_cap;
    char* meta;         /* SEARCH, optional: per key in `out`, four NUL terminated fields: title, artist, format, duration "m:ss" ("" unknown) */
    uint32_t meta_cap;
    uint32_t meta_used; /* out: bytes of `meta` written (whole rows) */
    uint32_t count;     /* out: keys stored (SEARCH), rows imported (IMPORT) */
    uint32_t truncated; /* out: keys that did not fit */
    uint32_t used_fts;  /* out: 1 when the FTS5 index served the query */
    char error[24];     /* out: "" or a query error code (unknown_key, unknown_view, ...) */
} sel_lib_job;

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

/* Rows to delete again (a changelog's deleted: and moved: entries): `count` paths of one source. */
typedef struct sel_remove_batch {
    const char* source;
    const char* const* paths;
    uint32_t count;
} sel_remove_batch;

/* Side data for songs that already have a row (HVSC Songlengths.md5 and STIL, I162). `duration_ms` > 0
 * and a non-empty `md5` replace the song's values; every sticker is stored under the song key in the
 * stickers table. A path without a songs row is skipped (not an error, not counted). */
typedef struct sel_sticker {
    const char* name;
    const char* value;
} sel_sticker;
typedef struct sel_annotation {
    const char* path;
    const char* md5;      /* "" or NULL = keep */
    int32_t duration_ms;  /* 0 = keep */
    const sel_sticker* stickers;
    uint32_t sticker_n;
} sel_annotation;
typedef struct sel_annotate_batch {
    const char* source;
    const sel_annotation* items;
    uint32_t count;
} sel_annotate_batch;
/* Reply to SEL_LIB_REQ_ANNOTATE: INT [0] songs annotated, [1] SQLite result code (0 = ok). */

/* Reply to SEL_LIB_REQ_REMOVE: INT [0] song rows deleted, [1] SQLite result code (0 = ok). */

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

/* Runs a request that carries a job (SEARCH, SAVE_VIEW, IMPORT). 1 on success; query errors are in job->error. */
int sel_library_run(sel_library* lib, uint32_t req, sel_lib_job* job);

/* LIBRARY_FETCH (I165, I166, I166a): `spec` is "<source>:<path>" with an optional "#md5=<hex>". The
 * song and the companions `rules` name (sel_companion_rules) land in the handle's file cache. 1 with
 * the cache path in `out` and the files delivered in `files`; 0 on failure; -1 when every mirror
 * delivered a body whose md5 differs. */
int sel_library_fetch(sel_library* lib, const char* spec, const char* rules, char* out, size_t cap, uint32_t* files);
/* Cache counters; 0 when the handle served no fetch. */
struct sel_cache_stats;
/* Trim the file cache to `budget` bytes (0 = the machine's default, sel_cache_budget) by last access.
 * Songs in the `kept` and `liked` tables are exempt. Runs after every LIBRARY_FETCH that stored a
 * file. Returns the trim_runs counter after the call; 0 when the handle has no cache dir. */
struct sel_cache_trim_result;
unsigned sel_library_cache_trim(sel_library* lib, uint32_t budget, struct sel_cache_trim_result* out);
/* Marks song key `key` in table "kept" or "liked" (seeding and tests). 1 on success. */
int sel_library_mark(sel_library* lib, const char* table, const char* key);
int sel_library_cache_stats(const sel_library* lib, struct sel_cache_stats* out);
/* Prints `cache_hits= fetches= companions= md5_mismatches= mirror_tries=` when a fetch was served. */
void sel_library_cache_report(const sel_library* lib);

uint32_t sel_library_account(const sel_library* lib);
uint32_t sel_library_heap_size(const sel_library* lib);
/* Heap allocations made so far by this library (1 after a successful open). */
uint32_t sel_library_heap_allocs(const sel_library* lib);
const char* sel_library_sqlite_version(void);

#endif
