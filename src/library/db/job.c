/* The library background job thread (I173d): the one thread behind prefetch, smart downloads and DEViLBOX rating
 * pushes. Before this there were two copies of the same scaffold (fetch.c, rating_push.c). The queue is fixed size
 * and lives in the library handle: one prefetch slot (pf_spec), SEL_SMART_MAX smart slots (sm_spec) and one rating
 * flush flag (job_rating), one vote and one lyrics request (job_vote, job_lyric); a repeated request overwrites or sets the same slot, so nothing is allocated per job and
 * a repeated rating flush coalesces into one. Order: rating flush first (short), then a prefetch, then smart
 * downloads. Close stops accepting work, wakes the thread and joins it; a job in flight ends at its next net poll slice
 * (sel_net_set_cancel on job_quit, ~50 ms; a download also at its next chunk, pf_cancel). Builds without threads (Amiga) run jobs inline, see fetch.c
 * (cooperative poll) and rating_push.c (inline flush). */
#include "db_internal.h"
#include "../net/net.h"

#if !defined(SEL_LIBRARY_NO_THREAD)

static void* job_main(void* arg) {
    sel_library* lib = arg;

    sel_net_set_cancel(&lib->job_quit, 0u); /* close ends a job stuck on a silent server within one poll slice (I173f) */
    pthread_mutex_lock(&lib->job_lock);
    for (;;) {
        while (!lib->job_quit && !lib->job_rating && !lib->job_vote && !lib->job_lyric && !lib->job_hash && !lib->job_task &&
               !sel_fetch_job_pending(lib)) {
            pthread_cond_wait(&lib->job_wake, &lib->job_lock);
        }
        if (lib->job_quit) {
            break;
        }
        if (lib->job_task) { /* I336: the one long caller task (the folder scan), run with the lock dropped */
            lib->job_task = 0;
            lib->job_task_busy = 1;
            pthread_mutex_unlock(&lib->job_lock);
            lib->task_fn(lib->task_ctx);
            pthread_mutex_lock(&lib->job_lock);
            lib->job_task_busy = 0;
            lib->task_done = 1;
            pthread_cond_broadcast(&lib->job_wake);
            continue;
        }
        if (lib->job_rating) {
            lib->job_rating = 0;
            lib->job_rating_busy = 1;
            pthread_mutex_unlock(&lib->job_lock);
            sel_rating_job_run(lib);
            pthread_mutex_lock(&lib->job_lock);
            lib->job_rating_busy = 0;
            pthread_cond_broadcast(&lib->job_wake);
            continue;
        }
        if (lib->job_vote) {
            lib->job_vote = 0;
            lib->job_vote_busy = 1;
            pthread_mutex_unlock(&lib->job_lock);
            sel_vote_job_run(lib);
            pthread_mutex_lock(&lib->job_lock);
            lib->job_vote_busy = 0;
            pthread_cond_broadcast(&lib->job_wake);
            continue;
        }
        if (lib->job_lyric) {
            lib->job_lyric = 0;
            lib->job_lyric_busy = 1;
            pthread_mutex_unlock(&lib->job_lock);
            sel_lyrics_job_run(lib);
            pthread_mutex_lock(&lib->job_lock);
            lib->job_lyric_busy = 0;
            pthread_cond_broadcast(&lib->job_wake);
            continue;
        }
        if (lib->job_hash && !sel_fetch_job_pending(lib)) {
            /* lowest priority: one file per turn, so a queued job never waits for the whole pass (I184a) */
            lib->job_hash = 0;
            lib->job_hash_busy = 1;
            sel_hash_job_run(lib);
            lib->job_hash_busy = 0;
            pthread_cond_broadcast(&lib->job_wake);
            continue;
        }
        sel_fetch_job_run(lib);
    }
    pthread_mutex_unlock(&lib->job_lock);
    return NULL;
}

int sel_job_start(sel_library* lib) {
    if (!lib->job_started) {
        pthread_mutex_init(&lib->job_lock, NULL);
        pthread_mutex_init(&lib->cache_lock, NULL);
        pthread_cond_init(&lib->job_wake, NULL);
        if (pthread_create(&lib->job_thread, NULL, job_main, lib) != 0) {
            return 0;
        }
        lib->job_started = 1;
    }
    return 1;
}

void sel_job_wake(sel_library* lib) {
    pthread_cond_broadcast(&lib->job_wake);
}

void sel_job_wait(sel_library* lib) {
    if (lib != NULL && lib->job_started) {
        pthread_mutex_lock(&lib->job_lock);
        while (lib->job_task || lib->job_task_busy || lib->job_rating || lib->job_rating_busy || lib->job_vote || lib->job_vote_busy || lib->job_lyric || lib->job_lyric_busy || lib->job_hash || lib->job_hash_busy || sel_fetch_job_pending(lib) || lib->pf_busy) {
            pthread_cond_wait(&lib->job_wake, &lib->job_lock);
        }
        pthread_mutex_unlock(&lib->job_lock);
    }
}

void sel_job_stop(sel_library* lib) {
    if (lib != NULL && lib->job_started) {
        pthread_mutex_lock(&lib->job_lock);
        lib->job_quit = 1;
        lib->task_cancel = 1;
        lib->pf_cancel = 1;
        pthread_cond_broadcast(&lib->job_wake);
        pthread_mutex_unlock(&lib->job_lock);
        pthread_join(lib->job_thread, NULL);
        lib->job_started = 0;
    }
}

/* I336: one long caller task (the folder scan) on the job thread, so the shell loop never blocks on it. _start returns 0
 * while a task runs or its end is not yet collected; _take returns 1 once per finished task (the caller then finishes
 * its work on its own thread); _cancelled is polled by the task between items; _cancel stops a running task and waits. */
int sel_library_task_start(sel_library* lib, void (*fn)(void* ctx), void* ctx) {
    int ok = 0;

    if (lib == NULL || !sel_job_start(lib)) {
        return 0;
    }
    pthread_mutex_lock(&lib->job_lock);
    if (!lib->job_task && !lib->job_task_busy && !lib->task_done) {
        lib->task_fn = fn;
        lib->task_ctx = ctx;
        lib->task_cancel = 0;
        lib->job_task = 1;
        sel_job_wake(lib);
        ok = 1;
    }
    pthread_mutex_unlock(&lib->job_lock);
    return ok;
}

int sel_library_task_take(sel_library* lib) {
    int r;

    if (lib == NULL || !lib->job_started) {
        return 0;
    }
    pthread_mutex_lock(&lib->job_lock);
    r = (int)lib->task_done;
    lib->task_done = 0;
    pthread_mutex_unlock(&lib->job_lock);
    return r;
}

int sel_library_task_cancelled(sel_library* lib) {
    return lib != NULL && (lib->task_cancel || lib->job_quit);
}

void sel_library_task_cancel(sel_library* lib) {
    if (lib != NULL && lib->job_started) {
        pthread_mutex_lock(&lib->job_lock);
        if (lib->job_task || lib->job_task_busy) {
            lib->task_cancel = 1;
        }
        while (lib->job_task || lib->job_task_busy) {
            pthread_cond_wait(&lib->job_wake, &lib->job_lock);
        }
        pthread_mutex_unlock(&lib->job_lock);
    }
}

#else

/* Builds without threads run the task inline; the caller collects it with _take on the same tick. */
int sel_library_task_start(sel_library* lib, void (*fn)(void* ctx), void* ctx) {
    if (lib == NULL || lib->task_done) {
        return 0;
    }
    lib->task_cancel = 0;
    fn(ctx);
    lib->task_done = 1;
    return 1;
}

int sel_library_task_take(sel_library* lib) {
    int r = lib != NULL && lib->task_done;

    if (lib != NULL) {
        lib->task_done = 0;
    }
    return r;
}

int sel_library_task_cancelled(sel_library* lib) {
    return lib != NULL && lib->task_cancel;
}

void sel_library_task_cancel(sel_library* lib) {
    (void)lib;
}

#endif
