/* The library background job thread (I173d): the one thread behind prefetch, smart downloads and DEViLBOX rating
 * pushes. Before this there were two copies of the same scaffold (fetch.c, rating_push.c). The queue is fixed size
 * and lives in the library handle: one prefetch slot (pf_spec), SEL_SMART_MAX smart slots (sm_spec) and one rating
 * flush flag (job_rating); a repeated request overwrites or sets the same slot, so nothing is allocated per job and
 * a repeated rating flush coalesces into one. Order: rating flush first (short), then a prefetch, then smart
 * downloads. Close stops accepting work, wakes the thread and joins it; a job in flight ends at the net timeout
 * (a download also at its next chunk, pf_cancel). Builds without threads (Amiga) run jobs inline, see fetch.c
 * (cooperative poll) and rating_push.c (inline flush). */
#include "db_internal.h"

#if !defined(SEL_LIBRARY_NO_THREAD)

static void* job_main(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->job_lock);
    for (;;) {
        while (!lib->job_quit && !lib->job_rating && !sel_fetch_job_pending(lib)) {
            pthread_cond_wait(&lib->job_wake, &lib->job_lock);
        }
        if (lib->job_quit) {
            break;
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
        while (lib->job_rating || lib->job_rating_busy || sel_fetch_job_pending(lib) || lib->pf_busy) {
            pthread_cond_wait(&lib->job_wake, &lib->job_lock);
        }
        pthread_mutex_unlock(&lib->job_lock);
    }
}

void sel_job_stop(sel_library* lib) {
    if (lib != NULL && lib->job_started) {
        pthread_mutex_lock(&lib->job_lock);
        lib->job_quit = 1;
        lib->pf_cancel = 1;
        pthread_cond_broadcast(&lib->job_wake);
        pthread_mutex_unlock(&lib->job_lock);
        pthread_join(lib->job_thread, NULL);
        lib->job_started = 0;
    }
}

#endif
