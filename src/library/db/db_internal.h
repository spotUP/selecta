/* Private to src/library/db/: the library handle shared by db.c and worker.c. */
#ifndef SELECTA_LIBRARY_DB_INTERNAL_H
#define SELECTA_LIBRARY_DB_INTERNAL_H

#include "db.h"
#include "../../engine/spsc.h"
#include "../query.h"
#include "../cache.h"

#include <sqlite3.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#include <pthread.h>
#endif

#define SEL_SMART_MAX 8u

struct sel_library {
    const sel_host* host;
    uint32_t account;
    sqlite3* db;
    sel_spsc requests; /* caller -> worker */
    sel_spsc results;  /* worker -> caller */
    uint32_t served;   /* requests handled; written by the worker only */
    uint32_t heap_allocs;
    uint32_t heap_bytes;
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
#if !defined(SEL_LIBRARY_NO_THREAD)
    pthread_t pf_thread;     /* prefetch thread, created on the first prefetch (fetch.c, I168) */
    pthread_mutex_t pf_lock; /* guards the pf_* fields and serialises the file cache between threads */
    pthread_cond_t pf_wake;
    pthread_mutex_t cache_lock; /* the file cache's stats and the foreground fetch; never held by the prefetch thread
                                 * during a transfer (I168a) */
    sel_cache* pf_cache;     /* the prefetch thread's own cache handle: same directory, own mirror state and stats */
    volatile int pf_cancel;  /* set (under pf_lock) to abort the running prefetch download at its next chunk */
    unsigned pf_started, pf_busy, pf_quit;
    unsigned pf_busy_smart;  /* the running request is a smart download (a prefetch request cancels it) */
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake_worker;
    pthread_cond_t wake_caller;
    int running;
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
void sel_library_annotate(sel_library* lib, const sel_annotate_batch* b, sel_message* reply);

/* worker.c */
int sel_worker_start(sel_library* lib);
void sel_worker_stop(sel_library* lib);

#endif
