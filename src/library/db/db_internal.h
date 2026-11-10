/* Private to src/library/db/: the library handle shared by db.c and worker.c. */
#ifndef SELECTA_LIBRARY_DB_INTERNAL_H
#define SELECTA_LIBRARY_DB_INTERNAL_H

#include "db.h"
#include "../../engine/spsc.h"
#include "../query.h"
#include "../cache.h"
#include "../md5.h"
#include "../sysfile_fetch.h"

#include <stdio.h>

#include <sqlite3.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#include <pthread.h>
#endif

#define SEL_SMART_MAX 8u

/* Test knob (I910, I906d): the env var names a delay in ms the worker sleeps before the query it guards, so a dui test can keep the worker
 * busy the way a real scan does. Absent on Amiga and unset in production. */
#if !defined(__amigaos__) && !defined(AMIGA)
void sel_library_test_slow(const char* var); /* test_slow.c: usleep needs _DEFAULT_SOURCE under -std=c99 on glibc (I990b) */
#else
#define sel_library_test_slow(var) ((void)0)
#endif

/* I406b: pinned search hits waiting for the job thread (pin.c). Buffers are sized like the engine's search_hit. */
enum { SEL_PIN_MAX = 32, SEL_PIN_PATH = 256, SEL_PIN_TEXT = 160, SEL_PIN_ARTIST = 96, SEL_PIN_FORMAT = 32, SEL_PIN_SOURCE = 24 };
typedef struct sel_pin_slot {
    char source[SEL_PIN_SOURCE];
    char path[SEL_PIN_PATH];
    char title[SEL_PIN_TEXT];
    char artist[SEL_PIN_ARTIST];
    char format[SEL_PIN_FORMAT];
} sel_pin_slot;


/* I961c: per-source song counts and sync times as the writer last saw them (srcmirror.c). seq is odd while the writer rewrites it. */
typedef struct sel_src_mirror {
    unsigned seq;
    uint32_t nsongs, nsync;
    struct { char id[16]; int n; } songs[32];
    struct { char id[16]; int64_t when; } sync[32];
} sel_src_mirror;

/* I710ga3: one system-file download request (fetch.c). state: SEL_SYSF_NONE free, SEL_SYSF_RUNNING queued or running,
 * else the finished result. The decl's strings live in the plugin descriptor. */
#define SEL_SYSF_SLOTS 4u
typedef struct sel_sysf_slot {
    sel_sysfile_decl decl;
    char config[256];
    int state;
} sel_sysf_slot;

struct sel_library {
    const sel_host* host;
    uint32_t account;
    sqlite3* db;
    int has_index;     /* a read-only index is attached as schema idx (index.c) */
    int idx_dup;       /* the attached index has dup_of / dup_members (exported at schema v18 or later; I1094c) */
    char src_off[160]; /* comma separated ids of the switched-off sources (library.sources_off, I265); read by search and radio */
    int hide_dups;     /* library.hide_duplicates (I1094c): searches and collection lists skip songs.dup_of IS NOT NULL; default on */
    sel_spsc requests; /* caller -> worker */
    sel_spsc results;  /* worker -> caller */
    uint32_t served;   /* requests handled; written by the worker only */
    uint32_t search_runs;         /* I1231: SEL_LIB_REQ_SEARCH requests run (sel_library_search) */
    uint32_t search_sources_runs; /* I1231: grouped catalogue searches run (sel_library_search_sources), the test sentinel */
    uint32_t heap_allocs;
    uint32_t heap_bytes;
    char fts_long[SEL_QUERY_TEXT_MAX + 8]; /* long FTS words of the running search, space separated */
    sel_lib_job* job;  /* the job of the request in flight; set by sel_library_run */
    sel_query query;   /* compile scratch, used by the worker only */
    char view_text[16][256]; /* saved queries handed to the compiler during one compile */
    unsigned view_used;
    sel_cache* cache;        /* file cache of LIBRARY_FETCH, created on the first fetch (fetch.c) */
    char rules[512];         /* companion rules last seen by a fetch (trim exempts companions, I167a) */
    uint32_t cache_budget;   /* bytes, scaled by the machine (sel_cache_budget) */
    unsigned fetch_requests; /* LIBRARY_FETCH requests served */
    char pf_spec[160];       /* pending prefetch request "<source>:<path>"; "" = none (I168, I168a) */
    char pf_rules[512];
    char sm_spec[SEL_SMART_MAX][160]; /* smart download queue (I169): lower priority than pf_spec */
    unsigned sm_n;
    unsigned sm_planned;     /* smart plans made (the handle reports smart_* once a plan ran) */
    char vote_token[40];     /* queued vote request (vote_push.c, I174); guarded by job_lock when threads exist */
    char vote_key[16];
    int vote_r;
    sel_vote_result vote_res;
    int lyr_local;           /* the queued song has a local .lrc: upload it after a definite 404 (I193b) */
    char lyr_path[512];      /* queued lyrics request: the song file whose md5 keys the endpoint (lyrics_fetch.c, I193) */
    sel_lyrics_result lyr_res;
    int64_t hash_after;      /* last song id the md5 pass hashed (duplicates.c, I184a); threads: under job_lock */
    unsigned hash_pending;   /* cooperative build: the idle md5 pass has songs left; threads: a kick restarts the
                              * pass from id 0 (under job_lock) */
    uint32_t hash_files, hash_chunks, hash_tick_max; /* sel_hash_stats (I184b) */
    int64_t scrape_after;    /* last albums.id the cover scraper looked at (art_scrape.c, I1003a); threads: under job_lock */
    unsigned scrape_pending; /* a pass has albums left; threads: a kick restarts it from id 0 (under job_lock) */
    int scrape_off;          /* set_enabled(0): kick does nothing */
    unsigned long scrape_mb_ms; /* sel_net_now_ms of the last MusicBrainz request, for the 1 request per second limit */
    sel_scrape_stats scrape_stats; /* under job_lock when threads exist */
    void (*task_fn)(void* ctx); /* I336: the queued caller task (job.c) */
    void* task_ctx;
    volatile unsigned task_cancel; /* also the net cancel generation while the task runs (I832), so a cancel ends its network wait */
    volatile unsigned write_gen;   /* I1077b: committed write transactions (commit hook, writer thread) */
    unsigned busy_waits;           /* I1162: busy-handler waits for another process's lock on the database file */
    volatile unsigned pin_gen;     /* I1077b: of those, commits made by pin batches */
    volatile unsigned in_pin;      /* I1077b: the writer is inside a pin batch */
    volatile unsigned touch_gen;   /* I1130a: of those, commits made by Recents touches (SEL_USER_LIBRARY_TOUCH: the opened table only) */
    volatile unsigned in_touch;    /* I1130a: the writer is inside a Recents touch */
    volatile unsigned in_cache;    /* I1231b: the writer is inside a search cache op; its commits are not library changes */
    volatile unsigned pin_rows;    /* I406b: rows written by pin batches (sel_library_pin_rows) */
    volatile unsigned pin_dropped; /* I406b: pins lost to a full ring */
    volatile unsigned rating_pending_n; /* I958: pending 'rating' pushes; the writer of pending_push keeps it, a UI read never asks the worker */
    sel_src_mirror src_mir;  /* I961c: written by the worker only */
    unsigned mir_dirty_songs; /* the update hook saw a songs row change; worker only */
    int mir_index;           /* has_index the songs part was computed for */
    unsigned task_done;      /* a task ended and _task_take has not collected it */
    uint32_t (*registry_count)(void); /* I1127b: copied into cache and pf_cache */
    const sel_tag* (*registry_get)(uint32_t);
    sel_sysf_slot sysf[SEL_SYSF_SLOTS]; /* I710ga3: under job_lock when threads exist */
    unsigned sysf_dups;                 /* requests ignored because that name was queued or running */
#if defined(SEL_LIBRARY_NO_THREAD)
    char pf_cur[160];        /* the request the poll is working on across ticks (I168b); "" = none */
    int pf_cur_smart;
    unsigned pf_cur_before;  /* cache stats.fetches when the request started */
#endif
#if !defined(SEL_LIBRARY_NO_THREAD)
    unsigned job_task, job_task_busy; /* I336: the queued caller task, and one running */
    unsigned job_vote, job_vote_busy; /* one queued vote request, and one running */
    unsigned job_lyric, job_lyric_busy; /* one queued lyrics request, and one running */
    unsigned job_sysf, job_sysf_busy;   /* I710ga3: queued system-file downloads, and one running (fetch.c) */
    unsigned job_scrape, job_scrape_busy; /* I1003a: cover scraper pass, one coalesced pending job and one running (art_scrape.c) */
    unsigned job_hash, job_hash_busy;   /* local md5 pass: one coalesced pending job, and one running (duplicates.c, I184a) */
    pthread_t job_thread;    /* the one background job thread, created on the first job (job.c, I173d) */
    pthread_mutex_t job_lock; /* guards the job queue (pf_spec, sm_spec, job_rating) and the flags below */
    pthread_cond_t job_wake;
    pthread_mutex_t cache_lock; /* the file cache's stats and the foreground fetch; never held by the job thread
                                 * during a transfer (I168a) */
    sel_cache* pf_cache;     /* the prefetch job's own cache handle: same directory, own mirror state and stats */
    volatile int pf_cancel;  /* set (under job_lock) to abort the running prefetch download at its next chunk */
    unsigned job_started, job_quit;
    unsigned pf_busy;        /* a prefetch / smart job is running */
    unsigned pf_busy_smart;  /* the running request is a smart download (a prefetch request cancels it) */
    unsigned job_pin, job_pin_busy;       /* I406b: pinned search hits queued in pin_q, and a drain running (pin.c) */
    sel_pin_slot pin_q[SEL_PIN_MAX];
    unsigned pin_n;
    unsigned job_rating, job_rating_busy; /* rating flush: one coalesced pending job, and one running (rating_push.c) */
    pthread_mutex_t call_lock; /* recursive: one caller at a time owns job and the reply queue (engine, push thread) */
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake_worker;
    pthread_cond_t wake_caller;
    int running;
    int in_handle; /* the worker is inside sel_library_handle (debug owner_check in worker.c) */
#else
    FILE* hash_file;         /* the file the idle md5 pass is reading, one chunk per poll (I184b); NULL = none */
    sel_md5 hash_md5;
    char hash_key[1200];
#endif
};

/* db.c: the one request handler; runs on the worker (or the caller's task without threads). */
void sel_library_handle(sel_library* lib, const sel_message* req, sel_message* reply);
void sel_user_rating_pending_refresh(sel_library* lib); /* I958: recounts lib->rating_pending_n */
void sel_library_search(sel_library* lib, sel_lib_job* job);
void sel_library_search_sources(sel_library* lib, sel_lib_job* job);
void sel_library_save_view(sel_library* lib, sel_lib_job* job);
void sel_library_import(sel_library* lib, sel_lib_job* job);
int sel_library_register_regexp(sqlite3* db);
/* I691c: the majority format of the songs behind one list item (SEL_LIB_ITEM_* kind and its key): the representative extension of the
 * platform family holding most of its songs (sel_platform_family_tally), "" when it has none. Runs on the library thread. */
/* I838: _cached answers from item_formats (schema v15) while the item's change key (items.c stamp_sql) holds, so a list reply scans no
 * songs and an unrelated write keeps the answer; on a miss it scans and stores. sel_db_majority_scans counts the scans (test sentinel). */
void sel_db_majority_format_cached(sqlite3* db, uint32_t kind, const char* key, char* out, size_t cap);
void sel_db_majority_format(sqlite3* db, uint32_t kind, const char* key, char* out, size_t cap);
unsigned long sel_db_majority_scans(void);

/* I848a: the two lookups every OPEN makes on the main thread's behalf (duplicates.c). Shared so the plan test reads the exact SQL. Both must be
 * index SEARCHes (schema v16: songs_md5 is partial on md5 <> '', so the query repeats that term; songs_source_path serves the path branch). */
#define SEL_OPEN_LOOKUP_SQL \
    "SELECT key, path, source = 'local', COALESCE(md5, ''), COALESCE(size, 0) FROM songs" \
    " WHERE key = ?1 OR (source = 'local' AND path = ?1) LIMIT 1"
/* Rank of a source in the D-M3-14 order, lower is better. */
#define SEL_SOURCE_RANK_SQL \
    "CASE source WHEN 'local' THEN 0 WHEN 'modland' THEN 3 WHEN 'hvsc' THEN 4 WHEN 'hippo' THEN 5 ELSE 6 END"
/* I1094a: THE ranking of the copies of one song (design 2026-11-06-I1094-hide-duplicates.md section 2), first difference wins: the
 * user's pick in dup_pick, source rank, larger size, lowest id. `t` is the songs alias (a string literal) the ORDER BY runs over; the
 * source rank stays unqualified, so it binds to the innermost songs. Every reader uses this: the maintained songs.dup_of
 * (sel_dup_regroup/sel_dup_rebuild), op_dup_next and SEL_BEST_SOURCE_SQL, so the copy a list shows is the copy OPEN plays. */
#define SEL_DUP_RANK_SQL(t) \
    "(SELECT COUNT(*) FROM dup_pick WHERE dup_pick.md5 = " t ".md5 AND dup_pick.key = " t ".key) DESC, " SEL_SOURCE_RANK_SQL \
    ", COALESCE(" t ".size, 0) DESC, " t ".id"
#define SEL_BEST_SOURCE_SQL \
    "SELECT key FROM songs WHERE md5 <> '' AND md5 = (SELECT md5 FROM songs WHERE key = ?1)" \
    " ORDER BY " SEL_DUP_RANK_SQL("songs") " LIMIT 1"

/* I1094a: maintained duplicate groups (schema v18). songs.dup_of is NULL on a unique song and on the best copy of a group, else the best
 * copy's key; songs.dup_members is the group size (1 when unique). sel_dup_rebuild recomputes every group in bounded slices (rowid slices,
 * then md5 slices of songs_md5: one UPDATE over every group ran the a060 heap dry, I1094b); the v18 migration runs it too.
 * sel_dup_regroup recomputes one md5 (index seek on songs_md5). Both run inside the writer's open transaction. Counters are test sentinels. */
int sel_dup_regroup(sqlite3* db, const char* md5);
int sel_dup_rebuild(sqlite3* db);
/* I1276: the `hvsc:` key owning the STIL stickers of a modland-HVSC or hvsc.c64.org URL key; any other key maps to itself. */
void sel_stil_owner_key(const char* key, char* out, size_t cap);
/* I1094b: the single-row writers' two halves. sel_dup_md5_of reads the md5 of song `key` ("" when no row or none) BEFORE the write;
 * sel_dup_after regroups that old md5 and the md5 `key` has AFTER the write (a delete leaves no row: only the old group moves). */
void sel_dup_md5_of(sqlite3* db, const char* key, char out[33]);
int sel_dup_after(sqlite3* db, const char* old_md5, const char* key);

/* I1094b: the same helpers over statements prepared once per bulk batch (scan batch). Zero-initialise, close at batch end. */
typedef struct sel_dup_stmts {
    sqlite3_stmt* md5;
    sqlite3_stmt* clear;
    sqlite3_stmt* set;
} sel_dup_stmts;
void sel_dup_stmts_close(sel_dup_stmts* d);
void sel_dup_md5_of_c(sqlite3* db, sel_dup_stmts* d, const char* key, char out[33]);
int sel_dup_regroup_c(sqlite3* db, sel_dup_stmts* d, const char* md5);
int sel_dup_after_c(sqlite3* db, sel_dup_stmts* d, const char* old_md5, const char* key);
unsigned long sel_db_dup_regroups(void);
unsigned long sel_db_dup_rebuilds(void);

/* scan.c: SEL_LIB_REQ_SCAN_BATCH. */
void sel_library_scan_batch(sel_library* lib, const sel_scan_batch* b, sel_message* reply);
void sel_library_remove(sel_library* lib, const sel_remove_batch* b, sel_message* reply);
void sel_library_move(sel_library* lib, const sel_move* m, sel_message* reply);
void sel_library_meta(sel_library* lib, const sel_meta_op* op, sel_message* reply);
void sel_library_srcstate(sel_library* lib, const sel_srcstate_op* op, sel_message* reply);
void sel_library_search_cache(sel_library* lib, const sel_searchcache_op* op, sel_message* reply);
void sel_library_annotate(sel_library* lib, const sel_annotate_batch* b, sel_message* reply);

/* art.c: SEL_LIB_REQ_ART. */
void sel_library_art_op(sel_library* lib, const sel_art_op* op, sel_message* reply);

/* genre.c: SEL_LIB_REQ_GENRE. */
void sel_library_genre_op(sel_library* lib, const sel_genre_op* op, sel_message* reply);
/* One genre-op round trip through the worker: the reply's [0] (1 = found/stored), -1 on a library error; *id = [2]. */
int sel_genre_call(sel_library* lib, const sel_genre_op* op, int64_t* id);
int sel_url_encode(const char* name, char* out, size_t cap);
int sel_library_http_json(const char* url, char* text, size_t cap, uint32_t* requests);
int sel_json_string(const char* json, const char* name, char* out, size_t cap);

/* songdb.c (I182): worker half of the SEL_SONGDB_* / SEL_COMPOSER_NEXT genre ops, and the caller half that
 * resolves one song (cache or endpoint, then song_composers). 0 on a library error. */
int sel_library_tag_composer(sqlite3* db, const char* key, const char* author); /* I296: scan's author tag row */
int sel_library_songdb_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id);
int sel_library_items_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id); /* I678a, items.c */
/* I678j: the Home feed's item shelves (items.c): SEL_HOME_QUICK .. SEL_HOME_PLAYLISTS, `limit` rows each; returns the rows or a sqlite error code. */
/* The item kind of a Home line that is a plain song (a stream or Modland play with no songs row), not a collection item. */
enum { SEL_HOME_ITEM_SONG = 255 };
enum { SEL_HOME_QUICK = 0, SEL_HOME_RECENT, SEL_HOME_ALBUMS, SEL_HOME_COMPOSERS, SEL_HOME_PLAYLISTS, SEL_HOME_SHELVES };
/* The Home chips (design doc section 11): All, Music (neither tracker nor chip formats), Modules (tracker formats), Chiptunes. */
/* Writes the SQL condition "song of the chip's format family" over `col` (a songs.format column expression) into out; "1" for All. */
void sel_home_chip_sql(uint32_t chip, const char* col, char* out, size_t cap);
typedef void (*sel_home_item_fn)(void* ctx, uint32_t kind, const char* key, const char* name, const char* sub);
int sel_home_items(sqlite3* db, uint32_t shelf, uint32_t chip, uint32_t limit, sel_home_item_fn fn, void* ctx);
int sel_library_collection_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id); /* I678f, items.c */
int sel_library_songdb_resolve(sel_library* lib, const char* ukey, const char* path, const char* song_key,
                               sel_genre_stats* stats);

/* similar.c (I183): worker half of SEL_SIMILAR_NEXT / SEL_ALSOBY_NEXT. 0 on a library error. */
int sel_library_similar_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id);

/* duplicates.c (I184): worker half of SEL_DUP_HASH / SEL_DUP_NEXT / SEL_BEST_SOURCE / SEL_DUP_PICK / SEL_DUP_STORE. 0 on a library error. */
int sel_library_duplicates_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id);
void sel_library_hash_close(sel_library* lib); /* closes the file of an unfinished idle md5 pass (no threads) */

/* user.c: SEL_LIB_REQ_USER. */
void sel_library_user(sel_library* lib, const sel_user_op* op, sel_message* reply);

/* I172: inside `INSERT ... ON CONFLICT DO UPDATE SET col = ...` on songs, keeps a user's tag edit (sticker
 * "tag.<col>") over the scanned value. Every songs upsert uses it, so the edit survives a rescan. */
#define SEL_SONG_TAG_KEEP(col) SEL_SONG_TAG_KEEP_AT(col, "excluded.key", "excluded." #col)
/* The same rule with the sticker key and the scanned value as SQL expressions, for a plain UPDATE (the MOVE
 * rekey, where the stickers still sit under the old key). */
#define SEL_SONG_TAG_KEEP_AT(col, key, value) \
    "COALESCE((SELECT value FROM stickers WHERE key = " key " AND name = 'tag." #col "'), " value ")"

/* worker.c */
int sel_worker_start(sel_library* lib);
void sel_worker_stop(sel_library* lib);

/* The library background job thread (job.c, I173d). Threaded builds only; job kinds: prefetch / smart download
 * (fetch.c, queue = pf_spec + sm_spec) and rating flush (rating_push.c, one coalesced flag). */
#if !defined(SEL_LIBRARY_NO_THREAD)
int sel_job_start(sel_library* lib);
void sel_lyrics_job_run(sel_library* lib);       /* lyrics_fetch.c: runs the queued lyrics request (job_lock NOT held) */
void sel_vote_job_run(sel_library* lib);        /* vote_push.c: runs the queued vote request (job_lock NOT held) */
void sel_scrape_job_run(sel_library* lib);      /* art_scrape.c: looks up the next album without a cover, requeues the pass while albums are left (job_lock held on entry and exit) */
void sel_hash_job_run(sel_library* lib);        /* duplicates.c: hashes the next unhashed local song and requeues the pass while songs are left (job_lock held on entry and exit; dropped around the file) */
void sel_job_wake(sel_library* lib);  /* job_lock held: a job was queued */
void sel_job_wait(sel_library* lib);  /* returns once the queue is empty and nothing runs */
void sel_job_stop(sel_library* lib);
int sel_fetch_job_pending(sel_library* lib);  /* job_lock held */
void sel_sysf_job_run(sel_library* lib);     /* job_lock held on entry and exit; one request per call */
void sel_fetch_job_run(sel_library* lib);     /* job_lock held on entry and exit; drops it around the transfer */
#endif
void sel_pin_job_run(sel_library* lib);       /* I406b: writes the queued pinned hits as one SCAN_BATCH (no lock held) */
void sel_rating_job_run(sel_library* lib);    /* sends every pending rating (no lock held) */

#endif

/* amiga_vfs.c: registers the AmigaDOS-path VFS as default (68k only; no-op on desktop). */
void sel_library_vfs_init(void);
