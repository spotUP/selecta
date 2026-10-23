/* Private to src/library/db/: the library handle shared by db.c and worker.c. */
#ifndef SELECTA_LIBRARY_DB_INTERNAL_H
#define SELECTA_LIBRARY_DB_INTERNAL_H

#include "db.h"
#include "../../engine/spsc.h"

#include <sqlite3.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#include <pthread.h>
#endif

struct sel_library {
    const sel_host* host;
    uint32_t account;
    sqlite3* db;
    sel_spsc requests; /* caller -> worker */
    sel_spsc results;  /* worker -> caller */
    uint32_t served;   /* requests handled; written by the worker only */
    uint32_t heap_allocs;
    uint32_t heap_bytes;
#if !defined(SEL_LIBRARY_NO_THREAD)
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake_worker;
    pthread_cond_t wake_caller;
    int running;
#endif
};

/* db.c: the one request handler; runs on the worker (or the caller's task without threads). */
void sel_library_handle(sel_library* lib, const sel_message* req, sel_message* reply);

/* scan.c: SEL_LIB_REQ_SCAN_BATCH. */
void sel_library_scan_batch(sel_library* lib, const sel_scan_batch* b, sel_message* reply);

/* worker.c */
int sel_worker_start(sel_library* lib);
void sel_worker_stop(sel_library* lib);

#endif
