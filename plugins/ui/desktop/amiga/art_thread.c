/* Amiga art worker (art.h): no thread is started, so art.c runs fn inline on the caller, the same path the mem backend
 * takes when a thread is not wanted. */
#include "../art.h"

void* art_thread_start(void (*fn)(void*), void* ctx) {
    (void)fn;
    (void)ctx;
    return NULL;
}

void art_thread_join(void* handle) { (void)handle; }
