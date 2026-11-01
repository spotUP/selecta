/* Library worker (plan Phase 1, I145; D-M3-2): the thread that owns the DB
 * connection, with a request and a result SPSC queue. Callers push a request
 * and wait for the reply; the queues were allocated once at open, so a call
 * allocates nothing. Without threads (Amiga until Phase 12) the request runs
 * on the caller's task through the same handler and queues. */
#include "db_internal.h"
#include "../../core/count.h"

#include <string.h>
#if !defined(SEL_LIBRARY_NO_THREAD) && !defined(NDEBUG)
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#endif

enum { QUEUE_SLOTS = 16 };

#if !defined(SEL_LIBRARY_NO_THREAD)
static __thread int g_nowait;
static uint32_t g_deferred;
static __thread int g_watch;     /* I829: this thread is the UI/command thread: every request it makes is counted */
static uint32_t g_watched_waits; /* requests a watched thread made that waited on the worker (sentinel: must stay 0) */
static uint32_t g_watched_wait_ms; /* I946: host clock ms the watched threads spent inside those requests (queue behind a job and the reply) */

int sel_library_nowait(int on) {
    int was = g_nowait;
    g_nowait = on != 0;
    return was;
}

int sel_library_watch(int on) {
    int was = g_watch;
    g_watch = on != 0;
    return was;
}

uint32_t sel_library_watched_waits(void) {
    return SEL_COUNT_GET(&g_watched_waits);
}

uint32_t sel_library_watched_wait_ms(void) {
    return SEL_COUNT_GET(&g_watched_wait_ms);
}

/* I906a: allow list for the debug UI-wait guard. A watched thread that must wait on the library wraps the call in
 * sel_library_ui_allow(1)/(0) and names why in a comment; the goal is zero such sites. */
static __thread int g_allow;
static uint32_t g_allowed_waits; /* waits a watched thread made inside an allow window */
static uint32_t g_guard_hits;    /* debug guard: waits a watched thread made outside nowait and outside the allow list */

int sel_library_ui_allow(int on) {
    int was = g_allow;
    g_allow = on != 0;
    return was;
}

uint32_t sel_library_allowed_waits(void) {
    return SEL_COUNT_GET(&g_allowed_waits);
}

uint32_t sel_library_ui_guard_hits(void) {
    return SEL_COUNT_GET(&g_guard_hits);
}

#if !defined(NDEBUG)
/* I906a (design 2026-11-01-ui-thread-library-waits.md section 3): debug builds say where the UI thread waits on the library.
 * `what` names the wait ("req", "art_join"). SELECTA_LIB_UI_GUARD=log (default) prints and counts, =abort also aborts. */
void sel_library_ui_wait_check(const char* what, uint32_t id) {
    void* frames[32];
    const char* mode;

    if (!g_watch || g_nowait) return;
    if (g_allow) {
        SEL_COUNT_ADD(&g_allowed_waits, 1u);
        return;
    }
    SEL_COUNT_ADD(&g_guard_hits, 1u);
    if (id != 0xFFFFFFFFu) {
        fprintf(stderr, "library_ui_wait: %s=%u\n", what, (unsigned)id);
    } else {
        fprintf(stderr, "library_ui_wait: %s\n", what);
    }
    backtrace_symbols_fd(frames, backtrace(frames, 32), STDERR_FILENO);
    mode = getenv("SELECTA_LIB_UI_GUARD");
    if (mode != NULL && strcmp(mode, "abort") == 0) abort();
}
#endif

uint32_t sel_library_deferred(void) {
    return SEL_COUNT_GET(&g_deferred);
}

int sel_library_busy(sel_library* lib) {
    if (pthread_mutex_trylock(&lib->call_lock) != 0) return 1;
    pthread_mutex_unlock(&lib->call_lock);
    return 0;
}

static void call_enter(sel_library* lib) {
    pthread_mutex_lock(&lib->call_lock);
}

static void call_leave(sel_library* lib) {
    pthread_mutex_unlock(&lib->call_lock);
}
#else
static void call_enter(sel_library* lib) {
    (void)lib;
}

static void call_leave(sel_library* lib) {
    (void)lib;
}
#endif

int sel_library_run(sel_library* lib, uint32_t req, sel_lib_job* job) {
    sel_message reply;
    int ok;

    call_enter(lib);
    lib->job = job; /* the caller blocks in sel_library_call, so the worker is the only reader */
    ok = sel_library_call(lib, req, &reply);
    lib->job = NULL;
    call_leave(lib);
    return ok;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

static void* worker_main(void* arg) {
    sel_library* lib = arg;
    sel_message req;
    sel_message reply;

    for (;;) {
        pthread_mutex_lock(&lib->lock);
        while (sel_spsc_count(&lib->requests) == 0 && lib->running) {
            pthread_cond_wait(&lib->wake_worker, &lib->lock);
        }
        pthread_mutex_unlock(&lib->lock);
        if (!sel_spsc_pop(&lib->requests, &req)) {
            if (!lib->running) {
                break;
            }
            continue;
        }
        if (req.id == SEL_LIB_REQ_STOP) {
            break;
        }
        memset(&reply, 0, sizeof reply);
        __atomic_store_n(&lib->in_handle, 1, __ATOMIC_RELEASE);
        sel_library_handle(lib, &req, &reply);
        __atomic_store_n(&lib->in_handle, 0, __ATOMIC_RELEASE);
        pthread_mutex_lock(&lib->lock);
        sel_spsc_push(&lib->results, &reply);
        pthread_cond_signal(&lib->wake_caller);
        pthread_mutex_unlock(&lib->lock);
    }
    return NULL;
}

#if !defined(NDEBUG)
/* Design 2026-10-31-interactive-path.md 1.1: SQLite is built THREADSAFE=0, so once the worker runs only it may touch the
 * connection. Debug builds check that at every statement prepare: a thread other than the worker that prepares a statement
 * while the worker is inside a request is two threads in sqlite on one connection, and aborts here with that thread's stack.
 * (Tests that seed rows on their own thread while the worker idles are not flagged.) */
static int owner_check(void* arg, int code, const char* a, const char* b, const char* c, const char* d) {
    sel_library* lib = arg;
    void* frames[32];

    (void)code, (void)a, (void)b, (void)c, (void)d;
    if (__atomic_load_n(&lib->in_handle, __ATOMIC_ACQUIRE) && !pthread_equal(pthread_self(), lib->thread)) {
        fprintf(stderr, "library_thread_violation: sqlite used off the library worker thread\n");
        backtrace_symbols_fd(frames, backtrace(frames, 32), STDERR_FILENO);
        abort();
    }
    return SQLITE_OK;
}
#endif

int sel_worker_start(sel_library* lib) {
    lib->running = 1;
    /* I790: g_nowait is a __thread variable. On macOS dyld allocates a thread's TLV block lazily at its first access, which
     * would be the opening thread's first sel_library_request, inside the SEL_ALLOC_FREEZE window after library start
     * (the freeze contract: no allocation once the library runs). Touch it here, on the thread that opens the library, so
     * the block is allocated at start. */
    (void)*(volatile int*)&g_nowait;
    (void)*(volatile int*)&g_watch;
    (void)*(volatile int*)&g_allow;
    if (!sel_spsc_init(&lib->requests, lib->host, QUEUE_SLOTS) || !sel_spsc_init(&lib->results, lib->host, QUEUE_SLOTS)) {
        return 0;
    }
    {
        pthread_mutexattr_t at;
        pthread_mutexattr_init(&at);
        pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&lib->call_lock, &at);
        pthread_mutexattr_destroy(&at);
    }
    pthread_mutex_init(&lib->lock, NULL);
    pthread_cond_init(&lib->wake_worker, NULL);
    pthread_cond_init(&lib->wake_caller, NULL);
    if (pthread_create(&lib->thread, NULL, worker_main, lib) != 0) return 0;
#if !defined(NDEBUG)
    sqlite3_set_authorizer(lib->db, owner_check, lib);
#endif
    return 1;
}

void sel_worker_stop(sel_library* lib) {
    pthread_mutex_lock(&lib->lock);
    lib->running = 0;
    pthread_cond_signal(&lib->wake_worker);
    pthread_mutex_unlock(&lib->lock);
    pthread_join(lib->thread, NULL);
    pthread_cond_destroy(&lib->wake_worker);
    pthread_cond_destroy(&lib->wake_caller);
    pthread_mutex_destroy(&lib->lock);
    pthread_mutex_destroy(&lib->call_lock);
    sel_spsc_free(&lib->requests, lib->host);
    sel_spsc_free(&lib->results, lib->host);
}

int sel_library_request(sel_library* lib, uint32_t req, const void* payload, sel_message* reply) {
    sel_message m;
    uint32_t t0 = g_watch && !g_nowait ? lib->host->clock_ms(lib->host) : 0u;

    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_CMD;
    m.id = req;
    if (payload != NULL) {
        m.argc = 1;
        m.args[0].type = SEL_ARG_STR;
        m.args[0].v.s = (const char*)payload;
    }
#if !defined(NDEBUG)
    sel_library_ui_wait_check("req", req);
#endif
    if (g_nowait) { /* I782: the call lock is held for a whole job and between a sync's requests; a draw never queues behind them */
        if (pthread_mutex_trylock(&lib->call_lock) != 0) {
            SEL_COUNT_ADD(&g_deferred, 1u);
            return 0;
        }
    } else {
        call_enter(lib);
    }
    /* I829: a watched thread is about to wait. A nowait request that got the call lock is not counted: with the lock free the
     * worker has no other request, so the reply is one statement away. */
    if (g_watch && !g_nowait) SEL_COUNT_ADD(&g_watched_waits, 1u);
    pthread_mutex_lock(&lib->lock);
    if (!sel_spsc_push(&lib->requests, &m)) {
        pthread_mutex_unlock(&lib->lock);
        call_leave(lib);
        return 0;
    }
    pthread_cond_signal(&lib->wake_worker);
    while (!sel_spsc_pop(&lib->results, reply)) {
        pthread_cond_wait(&lib->wake_caller, &lib->lock);
    }
    pthread_mutex_unlock(&lib->lock);
    call_leave(lib);
    if (g_watch && !g_nowait) SEL_COUNT_ADD(&g_watched_wait_ms, lib->host->clock_ms(lib->host) - t0);
    return 1;
}

#else

int sel_library_nowait(int on) {
    (void)on;
    return 0;
}

uint32_t sel_library_deferred(void) {
    return 0u;
}

/* Without a worker thread every request runs inline: there is no other thread to wait on. */
int sel_library_watch(int on) {
    (void)on;
    return 0;
}

uint32_t sel_library_watched_waits(void) {
    return 0u;
}

uint32_t sel_library_watched_wait_ms(void) {
    return 0u;
}

int sel_library_ui_allow(int on) {
    (void)on;
    return 0;
}

uint32_t sel_library_allowed_waits(void) {
    return 0u;
}

uint32_t sel_library_ui_guard_hits(void) {
    return 0u;
}

int sel_library_busy(sel_library* lib) {
    (void)lib;
    return 0;
}

int sel_worker_start(sel_library* lib) {
    return sel_spsc_init(&lib->requests, lib->host, QUEUE_SLOTS) && sel_spsc_init(&lib->results, lib->host, QUEUE_SLOTS);
}

void sel_worker_stop(sel_library* lib) {
    sel_spsc_free(&lib->requests, lib->host);
    sel_spsc_free(&lib->results, lib->host);
}

int sel_library_request(sel_library* lib, uint32_t req, const void* payload, sel_message* reply) {
    sel_message m;

    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_CMD;
    m.id = req;
    if (payload != NULL) {
        m.argc = 1;
        m.args[0].type = SEL_ARG_STR;
        m.args[0].v.s = (const char*)payload;
    }
    if (!sel_spsc_push(&lib->requests, &m) || !sel_spsc_pop(&lib->requests, &m)) {
        return 0;
    }
    memset(reply, 0, sizeof *reply);
    sel_library_handle(lib, &m, reply);
    return 1;
}

#endif

int sel_library_call(sel_library* lib, uint32_t req, sel_message* reply) {
    return sel_library_request(lib, req, NULL, reply);
}
