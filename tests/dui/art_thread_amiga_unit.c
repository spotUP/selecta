/* Unit test of the Amiga art worker (I369): art_thread_start has no thread to start, so the contract in art.h says the
 * job has run inline when it returns NULL. The Amiga build once returned NULL without running it, so no cover was ever
 * loaded on the rig (art_decodes=0) while the host test, which uses the posix worker, passed. */
#include "art.h"

#include <stdio.h>

static int ran;

static void job(void* ctx) {
    ran += *(int*)ctx;
}

int main(void) {
    int arg = 1;
    void* h = art_thread_start(job, &arg);

    art_thread_join(h);
    if (h != NULL || ran != 1) {
        fprintf(stderr, "art_thread_amiga_unit: handle=%p ran=%d (want NULL, 1)\n", h, ran);
        return 1;
    }
    return 0;
}
