/* Private to src/library/db/: the library handle shared by db.c and worker.c. */
#ifndef SELECTA_LIBRARY_DB_INTERNAL_H
#define SELECTA_LIBRARY_DB_INTERNAL_H

#include "db.h"
#include "../../engine/spsc.h"
#include "../query.h"
#include "../cache.h"
#include "../md5.h"

#include <stdio.h>

#include <sqlite3.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#include <pthread.h>
#endif

#define SEL_SMART_MAX 8u

struct sel_library {
    const sel_host* host;
    uint32_t account;
    sqlite3* db;
    int has_index;     /* a read-only index is attached as schema idx (index.c) */
    char src_off[160]; /* comma separated ids of the switched-off sources (library.sources_off, I265); read by search and radio */
    sel_spsc requests; /* caller -> worker */
    sel_spsc results;  /* worker -> caller */
    uint32_t served;   /* requests handled; written by the worker only */
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
    void (*task_fn)(void* ctx); /* I336: the queued caller task (job.c) */
    void* task_ctx;
    volatile int task_cancel;
    unsigned task_done;      /* a task ended and _task_take has not collected it */
#if defined(SEL_LIBRARY_NO_THREAD)
    char pf_cur[160];        /* the request the poll is working on across ticks (I168b); "" = none */
    int pf_cur_smart;
    unsigned pf_cur_before;  /* cache stats.fetches when the request started */
#endif
#if !defined(SEL_LIBRARY_NO_THREAD)
    unsigned job_task, job_task_busy; /* I336: the queued caller task, and one running */
    unsigned job_vote, job_vote_busy; /* one queued vote request, and one running */
    unsigned job_lyric, job_lyric_busy; /* one queued lyrics request, and one running */
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
    unsigned job_rating, job_rating_busy; /* rating flush: one coalesced pending job, and one running (rating_push.c) */
    pthread_mutex_t call_lock; /* recursive: one caller at a time owns job and the reply queue (engine, push thread) */
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake_worker;
    pthread_cond_t wake_caller;
    int running;
#else
    FILE* hash_file;         /* the file the idle md5 pass is reading, one chunk per poll (I184b); NULL = none */
    sel_md5 hash_md5;
    char hash_key[1200];
#endif
};

/* db.c: the one request handler; runs on the worker (or the caller's task without threads). */
void sel_library_handle(sel_library* lib, const sel_message* req, sel_message* reply);
void sel_library_search(sel_library* lib, sel_lib_job* job);
void sel_library_save_view(sel_library* lib, sel_lib_job* job);
void sel_library_import(sel_library* lib, sel_lib_job* job);
int sel_library_register_regexp(sqlite3* db);

/* scan.c: SEL_LIB_REQ_SCAN_BATCH. */
void sel_library_scan_batch(sel_library* lib, const sel_scan_batch* b, sel_message* reply);
void sel_library_remove(sel_library* lib, const sel_remove_batch* b, sel_message* reply);
void sel_library_move(sel_library* lib, const sel_move* m, sel_message* reply);
void sel_library_meta(sel_library* lib, const sel_meta_op* op, sel_message* reply);
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
void sel_hash_job_run(sel_library* lib);        /* duplicates.c: hashes the next unhashed local song and requeues the pass while songs are left (job_lock held on entry and exit; dropped around the file) */
void sel_job_wake(sel_library* lib);  /* job_lock held: a job was queued */
void sel_job_wait(sel_library* lib);  /* returns once the queue is empty and nothing runs */
void sel_job_stop(sel_library* lib);
int sel_fetch_job_pending(sel_library* lib);  /* job_lock held */
void sel_fetch_job_run(sel_library* lib);     /* job_lock held on entry and exit; drops it around the transfer */
#endif
void sel_rating_job_run(sel_library* lib);    /* sends every pending rating (no lock held) */

#endif

/* amiga_vfs.c: registers the AmigaDOS-path VFS as default (68k only; no-op on desktop). */
void sel_library_vfs_init(void);
