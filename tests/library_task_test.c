/* I336: the library job thread runs one long caller task (the folder scan) off the caller's thread. _start returns while the
 * task still runs, a second _start is refused until the end is collected, _take reports the end once, and _cancel stops a
 * task that polls _cancelled and waits for it. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

typedef struct task_ctx {
    sel_library* lib;
    volatile int release;
    volatile int ran;
    pthread_t where;
} task_ctx;

static void nap(void) {
    struct timespec ts = {0, 1000000L};
    nanosleep(&ts, NULL);
}

static void gated_task(void* p) {
    task_ctx* c = p;

    c->where = pthread_self();
    while (!c->release) {
        nap();
    }
    c->ran = 1;
}

static void polling_task(void* p) {
    task_ctx* c = p;

    while (!sel_library_task_cancelled(c->lib)) {
        nap();
    }
    c->ran = 1;
}

int main(void) {
    sel_library* lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    task_ctx c;
    int i, taken = 0;

    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    memset(&c, 0, sizeof c);
    c.lib = lib;
    check(sel_library_task_start(lib, gated_task, &c) == 1, "start accepted");
    check(c.ran == 0, "start returned before the task ended");
    check(sel_library_task_take(lib) == 0, "not finished yet");
    check(sel_library_task_start(lib, gated_task, &c) == 0, "second start refused while one runs");
    c.release = 1;
    for (i = 0; i < 5000 && !taken; i++) {
        taken = sel_library_task_take(lib);
        if (!taken) nap();
    }
    check(taken == 1 && c.ran == 1, "end collected once the task finished");
    check(!pthread_equal(c.where, pthread_self()), "the task ran on another thread");
    check(sel_library_task_take(lib) == 0, "the end is reported once");

    memset(&c, 0, sizeof c);
    c.lib = lib;
    check(sel_library_task_start(lib, polling_task, &c) == 1, "start after collect");
    sel_library_task_cancel(lib);
    check(c.ran == 1, "cancel waited for the task");
    check(sel_library_task_take(lib) == 1, "cancelled task end collected");

    sel_library_close(lib);
    if (fails == 0) printf("library_task_ok\n");
    return fails == 0 ? 0 : 1;
}
