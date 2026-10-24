/* POSIX worker thread for the artwork loader (DUI-P2-4): the only file of the desktop plugin with thread headers. */
#include "../art.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct job {
    pthread_t thread;
    void (*fn)(void*);
    void* ctx;
} job;

static void* run(void* p) {
    job* j = (job*)p;
    j->fn(j->ctx);
    return NULL;
}

void* art_thread_start(void (*fn)(void*), void* ctx) {
    job* j = (job*)malloc(sizeof *j);
    if (j != NULL) {
        j->fn = fn;
        j->ctx = ctx;
        if (pthread_create(&j->thread, NULL, run, j) == 0) {
            return j;
        }
        free(j);
    }
    fn(ctx);
    return NULL;
}

void art_thread_join(void* handle) {
    job* j = (job*)handle;
    if (j != NULL) {
        pthread_join(j->thread, NULL);
        free(j);
    }
}
