/* Amiga art worker (art.h): there is no thread, so the job runs inline on the caller and no handle is returned (the contract of art_thread_start). */
#include "../art.h"

void* art_thread_start(void (*fn)(void*), void* ctx) {
    fn(ctx);
    return NULL;
}

void art_thread_join(void* handle) { (void)handle; }
