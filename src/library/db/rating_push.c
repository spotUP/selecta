/* DEViLBOX rating pushes off the engine command thread (I173c). RATE stores the rating in the library (fast, the
 * worker), then queues a rating flush job on the library job thread (job.c, I173d); an unreachable server stalls only
 * that thread, never an engine command. Requests while one is pending coalesce into the one flag. The job talks to
 * the DB only through the library worker (sel_library_rating_pending / _done), which serialises callers with
 * call_lock. Without threads (Amiga until Phase 12) the push runs on the caller's task. */
#include "db_internal.h"

#include "../net/devilbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The likes in the same queue (I306): payload "token mkey r", sent through sel_devilbox_vote. A refused vote (bad key
 * or token) is dropped like a rejected rating; an unreachable or rate limiting service keeps the row for the next run. */
static void like_flush(sel_library* lib) {
    char buf[2048];
    uint32_t rows = 0;
    const char* p = buf;

    if (!sel_library_like_pending(lib, buf, sizeof buf, &rows) || rows == 0) {
        return;
    }
    while (*p != '\0') {
        const char* key = p;
        const char* payload = key + strlen(key) + 1;
        char token[40], mkey[16];
        int r = 0;

        if (sscanf(payload, "%39s %15s %d", token, mkey, &r) == 3) {
            int st = sel_devilbox_vote(token, mkey, r, NULL, NULL);

            if (st != SEL_VOTE_OK && st != SEL_VOTE_BAD) {
                break;
            }
        }
        (void)sel_library_like_done(lib, key);
        p = payload + strlen(payload) + 1;
    }
}

/* Sends every rating still waiting for DEViLBOX (D-M3-12), oldest first. A rejected push (400) is dropped; one that
 * cannot reach the server stays and ends the pass (the next run retries). */
void sel_rating_job_run(sel_library* lib) {
    char buf[2048];
    uint32_t rows = 0;
    const char* p = buf;

    like_flush(lib);
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

void sel_library_like_push(sel_library* lib, const char* key, const char* token, const char* mkey, int r) {
    char payload[96];

    if (lib == NULL || key == NULL || token == NULL || mkey == NULL ||
        snprintf(payload, sizeof payload, "%s %s %d", token, mkey, r) >= (int)sizeof payload) {
        return;
    }
    if (sel_library_like_queue(lib, key, payload)) {
        sel_library_rating_push(lib);
    }
}

#if !defined(SEL_LIBRARY_NO_THREAD)

void sel_library_rating_push(sel_library* lib) {
    if (lib == NULL) {
        return;
    }
    if (!sel_job_start(lib)) {
        sel_rating_job_run(lib); /* no thread to be had: send here rather than lose the push */
        return;
    }
    pthread_mutex_lock(&lib->job_lock);
    lib->job_rating = 1;
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
}

void sel_library_rating_wait(sel_library* lib) {
    sel_job_wait(lib);
}

#else

void sel_library_rating_push(sel_library* lib) {
    if (lib != NULL) {
        sel_rating_job_run(lib);
    }
}

void sel_library_rating_wait(sel_library* lib) {
    (void)lib;
}

#endif
