/* DEViLBOX rating pushes off the engine command thread (I173c). RATE stores the rating in the library (fast, the
 * worker), then asks this thread to send what is pending; an unreachable server stalls only this thread, never an
 * engine command. Same shape as the prefetch thread of fetch.c: one request flag, one thread, wait and stop hooks.
 * The thread talks to the DB only through the library worker (sel_library_rating_pending / _done), which serialises
 * callers with call_lock. Without threads (Amiga until Phase 12) the push runs on the caller's task. */
#include "db_internal.h"

#include "../net/devilbox.h"

#include <stdlib.h>
#include <string.h>

/* Sends every rating still waiting for DEViLBOX (D-M3-12), oldest first. A rejected push (400) is dropped; one that
 * cannot reach the server stays and ends the pass (the next run retries). */
static void flush_pending(sel_library* lib) {
    char buf[2048];
    uint32_t rows = 0;
    const char* p = buf;

    if (!sel_library_rating_pending(lib, buf, sizeof buf, &rows) || rows == 0) {
        return;
    }
    while (*p != '\0') {
        const char* key = p;
        const char* stars = key + strlen(key) + 1;
        int r = sel_devilbox_rating_push(key, atoi(stars));

        if (r == 0) {
            break;
        }
        (void)sel_library_rating_done(lib, key);
        p = stars + strlen(stars) + 1;
    }
}

#if !defined(SEL_LIBRARY_NO_THREAD)

static void* push_main(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->rp_lock);
    for (;;) {
        while (!lib->rp_quit && !lib->rp_work) {
            pthread_cond_wait(&lib->rp_wake, &lib->rp_lock);
        }
        if (lib->rp_quit) {
            break;
        }
        lib->rp_work = 0;
        lib->rp_busy = 1;
        pthread_mutex_unlock(&lib->rp_lock);
        flush_pending(lib);
        pthread_mutex_lock(&lib->rp_lock);
        lib->rp_busy = 0;
        pthread_cond_broadcast(&lib->rp_wake);
    }
    pthread_mutex_unlock(&lib->rp_lock);
    return NULL;
}

void sel_library_rating_push(sel_library* lib) {
    if (lib == NULL) {
        return;
    }
    if (!lib->rp_started) {
        pthread_mutex_init(&lib->rp_lock, NULL);
        pthread_cond_init(&lib->rp_wake, NULL);
        if (pthread_create(&lib->rp_thread, NULL, push_main, lib) != 0) {
            flush_pending(lib); /* no thread to be had: send here rather than lose the push */
            return;
        }
        lib->rp_started = 1;
    }
    pthread_mutex_lock(&lib->rp_lock);
    lib->rp_work = 1;
    pthread_cond_broadcast(&lib->rp_wake);
    pthread_mutex_unlock(&lib->rp_lock);
}

void sel_library_rating_wait(sel_library* lib) {
    if (lib != NULL && lib->rp_started) {
        pthread_mutex_lock(&lib->rp_lock);
        while (lib->rp_work || lib->rp_busy) {
            pthread_cond_wait(&lib->rp_wake, &lib->rp_lock);
        }
        pthread_mutex_unlock(&lib->rp_lock);
    }
}

void sel_library_rating_stop(sel_library* lib) {
    if (lib != NULL && lib->rp_started) {
        pthread_mutex_lock(&lib->rp_lock);
        lib->rp_quit = 1;
        pthread_cond_broadcast(&lib->rp_wake);
        pthread_mutex_unlock(&lib->rp_lock);
        pthread_join(lib->rp_thread, NULL);
        lib->rp_started = 0;
    }
}

#else

void sel_library_rating_push(sel_library* lib) {
    if (lib != NULL) {
        flush_pending(lib);
    }
}

void sel_library_rating_wait(sel_library* lib) {
    (void)lib;
}

void sel_library_rating_stop(sel_library* lib) {
    (void)lib;
}

#endif
