/* HippoPlayer votes off the engine command thread (I174). VOTE queues one request in the library handle (token, UADE
 * key, r) and wakes the library job thread (job.c, I173d), the thread that also flushes ratings; the HTTP GET to
 * /api/hip/vote or /api/hip/votes runs there, so an unreachable server stalls only that thread. A newer request
 * replaces one still queued (the sticker "vote" holds the user's own choice, the service holds the count). The
 * outcome is kept in the handle (sel_library_vote_result). Without threads (Amiga until Phase 12) the request runs on the caller's task. */
#include "db_internal.h"

#include "../net/devilbox.h"

#include <stdio.h>
#include <string.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#define VOTE_LOCK(l) pthread_mutex_lock(&(l)->job_lock)
#define VOTE_UNLOCK(l) pthread_mutex_unlock(&(l)->job_lock)
#else
#define VOTE_LOCK(l) ((void)(l))
#define VOTE_UNLOCK(l) ((void)(l))
#endif

void sel_vote_job_run(sel_library* lib) {
    char token[sizeof lib->vote_token];
    char mkey[sizeof lib->vote_key];
    sel_vote_result res;
    int r;

    VOTE_LOCK(lib);
    memcpy(token, lib->vote_token, sizeof token);
    memcpy(mkey, lib->vote_key, sizeof mkey);
    r = lib->vote_r;
    res = lib->vote_res; /* a refused or failed request keeps the last count the service gave for this song */
    VOTE_UNLOCK(lib);

    if (strncmp(res.mkey, mkey, 12) != 0) {
        memset(&res, 0, sizeof res);
    }
    res.done = 1;
    res.seq++;
    res.status = sel_devilbox_vote(token, mkey, r, &res.count, &res.yours);
    memcpy(res.mkey, mkey, sizeof res.mkey - 1u);
    VOTE_LOCK(lib);
    lib->vote_res = res;
    VOTE_UNLOCK(lib);
}

const char* sel_vote_status_name(int status) {
    switch (status) {
    case SEL_VOTE_OK: return "ok";
    case SEL_VOTE_RATE: return "rate";
    case SEL_VOTE_BAD: return "bad";
    case SEL_VOTE_MISSING: return "missing";
    default: return "down";
    }
}

void sel_library_vote_result(sel_library* lib, sel_vote_result* out) {
    memset(out, 0, sizeof *out);
    if (lib == NULL) return;
#if !defined(SEL_LIBRARY_NO_THREAD)
    if (lib->job_started) {
        VOTE_LOCK(lib);
        *out = lib->vote_res;
        VOTE_UNLOCK(lib);
        return;
    }
#endif
    *out = lib->vote_res;
}

static int vote_set(sel_library* lib, const char* token, const char* mkey, int r) {
    if (strlen(token) >= sizeof lib->vote_token || strlen(mkey) >= sizeof lib->vote_key) return 0;
    strcpy(lib->vote_token, token);
    strcpy(lib->vote_key, mkey);
    lib->vote_r = r;
    return 1;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

void sel_library_vote(sel_library* lib, const char* token, const char* mkey, int r) {
    if (lib == NULL || token == NULL || mkey == NULL) return;
    if (!sel_job_start(lib)) {
        if (vote_set(lib, token, mkey, r)) sel_vote_job_run(lib); /* no thread to be had: send here */
        return;
    }
    pthread_mutex_lock(&lib->job_lock);
    if (vote_set(lib, token, mkey, r)) {
        lib->job_vote = 1;
        sel_job_wake(lib);
    }
    pthread_mutex_unlock(&lib->job_lock);
}

#else

void sel_library_vote(sel_library* lib, const char* token, const char* mkey, int r) {
    if (lib != NULL && token != NULL && mkey != NULL && vote_set(lib, token, mkey, r)) sel_vote_job_run(lib);
}

#endif
