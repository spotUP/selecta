/* Library worker (plan Phase 1, I145; D-M3-2): the thread that owns the DB
 * connection, with a request and a result SPSC queue. Callers push a request
 * and wait for the reply; the queues were allocated once at open, so a call
 * allocates nothing. Without threads (Amiga until Phase 12) the request runs
 * on the caller's task through the same handler and queues. */
#include "db_internal.h"

#include <string.h>

enum { QUEUE_SLOTS = 16 };

int sel_library_run(sel_library* lib, uint32_t req, sel_lib_job* job) {
    sel_message reply;
    int ok;

    lib->job = job; /* the caller blocks in sel_library_call, so the worker is the only reader */
    ok = sel_library_call(lib, req, &reply);
    lib->job = NULL;
    return ok;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

static void* worker_main(void* arg) {
    sel_library* lib = arg;
    sel_message req;
    sel_message reply;

    for (;;) {
        pthread_mutex_lock(&lib->lock);
        while (sel_spsc_count(&lib->requests) == 0 && lib->running) {
            pthread_cond_wait(&lib->wake_worker, &lib->lock);
        }
        pthread_mutex_unlock(&lib->lock);
        if (!sel_spsc_pop(&lib->requests, &req)) {
            if (!lib->running) {
                break;
            }
            continue;
        }
        if (req.id == SEL_LIB_REQ_STOP) {
            break;
        }
        memset(&reply, 0, sizeof reply);
        sel_library_handle(lib, &req, &reply);
        pthread_mutex_lock(&lib->lock);
        sel_spsc_push(&lib->results, &reply);
        pthread_cond_signal(&lib->wake_caller);
        pthread_mutex_unlock(&lib->lock);
    }
    return NULL;
}

int sel_worker_start(sel_library* lib) {
    lib->running = 1;
    if (!sel_spsc_init(&lib->requests, lib->host, QUEUE_SLOTS) || !sel_spsc_init(&lib->results, lib->host, QUEUE_SLOTS)) {
        return 0;
    }
    pthread_mutex_init(&lib->lock, NULL);
    pthread_cond_init(&lib->wake_worker, NULL);
    pthread_cond_init(&lib->wake_caller, NULL);
    return pthread_create(&lib->thread, NULL, worker_main, lib) == 0;
}

void sel_worker_stop(sel_library* lib) {
    pthread_mutex_lock(&lib->lock);
    lib->running = 0;
    pthread_cond_signal(&lib->wake_worker);
    pthread_mutex_unlock(&lib->lock);
    pthread_join(lib->thread, NULL);
    pthread_cond_destroy(&lib->wake_worker);
    pthread_cond_destroy(&lib->wake_caller);
    pthread_mutex_destroy(&lib->lock);
    sel_spsc_free(&lib->requests, lib->host);
    sel_spsc_free(&lib->results, lib->host);
}

int sel_library_call(sel_library* lib, uint32_t req, sel_message* reply) {
    sel_message m;

    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_CMD;
    m.id = req;
    pthread_mutex_lock(&lib->lock);
    if (!sel_spsc_push(&lib->requests, &m)) {
        pthread_mutex_unlock(&lib->lock);
        return 0;
    }
    pthread_cond_signal(&lib->wake_worker);
    while (!sel_spsc_pop(&lib->results, reply)) {
        pthread_cond_wait(&lib->wake_caller, &lib->lock);
    }
    pthread_mutex_unlock(&lib->lock);
    return 1;
}

#else

int sel_worker_start(sel_library* lib) {
    return sel_spsc_init(&lib->requests, lib->host, QUEUE_SLOTS) && sel_spsc_init(&lib->results, lib->host, QUEUE_SLOTS);
}

void sel_worker_stop(sel_library* lib) {
    sel_spsc_free(&lib->requests, lib->host);
    sel_spsc_free(&lib->results, lib->host);
}

int sel_library_call(sel_library* lib, uint32_t req, sel_message* reply) {
    sel_message m;

    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_CMD;
    m.id = req;
    if (!sel_spsc_push(&lib->requests, &m) || !sel_spsc_pop(&lib->requests, &m)) {
        return 0;
    }
    memset(reply, 0, sizeof *reply);
    sel_library_handle(lib, &m, reply);
    return 1;
}

#endif
