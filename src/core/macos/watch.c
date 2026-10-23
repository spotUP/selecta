/* macOS folder watcher on FSEvents (file-level events). Declared in src/library/watch.h. The stream
 * runs on its own dispatch queue and appends changed directories to a mutex-guarded table; the pump
 * thread drains it in sel_watch_poll. FSEvents reports real paths (/private/var for /var), so the
 * real root prefix is mapped back to the spelling the caller gave. */
#include "../../library/watch.h"

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WATCH_DIRS 64u
#define WATCH_SETTLE_US 150000u
#define WATCH_STEP_US 10000u

struct sel_watch {
    FSEventStreamRef stream;
    dispatch_queue_t queue;
    pthread_mutex_t lock;
    char root[PATH_MAX];
    char real[PATH_MAX];
    char dirs[WATCH_DIRS][PATH_MAX];
    uint32_t dir_n, events;
    int overflow; /* more directories than the table holds: the root is reported instead */
};

static void note_dir(sel_watch* w, const char* dir) {
    uint32_t i;

    for (i = 0; i < w->dir_n; i++) {
        if (strcmp(w->dirs[i], dir) == 0) {
            return;
        }
    }
    if (w->dir_n < WATCH_DIRS) {
        strncpy(w->dirs[w->dir_n], dir, PATH_MAX - 1u);
        w->dirs[w->dir_n][PATH_MAX - 1u] = '\0';
        w->dir_n++;
    } else {
        w->overflow = 1;
    }
}

static void on_events(ConstFSEventStreamRef s, void* info, size_t n, void* paths,
                      const FSEventStreamEventFlags flags[], const FSEventStreamEventId ids[]) {
    sel_watch* w = info;
    char** p = paths;
    size_t i, rl = strlen(w->real);

    (void)s;
    (void)ids;
    pthread_mutex_lock(&w->lock);
    for (i = 0; i < n; i++) {
        char dir[PATH_MAX];
        char* slash;
        int is_dir = (flags[i] & kFSEventStreamEventFlagItemIsDir) != 0;

        if (strncmp(p[i], w->real, rl) != 0 || strlen(p[i]) - rl + strlen(w->root) >= PATH_MAX) {
            continue;
        }
        snprintf(dir, sizeof dir, "%s%s", w->root, p[i] + rl);
        if (!is_dir && (slash = strrchr(dir, '/')) != NULL && slash != dir) {
            *slash = '\0'; /* a file event changes its parent directory */
        }
        w->events++;
        note_dir(w, dir);
    }
    pthread_mutex_unlock(&w->lock);
}

void sel_watch_close(sel_watch* w) {
    if (w == NULL) {
        return;
    }
    if (w->stream != NULL) {
        FSEventStreamStop(w->stream);
        FSEventStreamInvalidate(w->stream);
        FSEventStreamRelease(w->stream);
    }
    if (w->queue != NULL) {
        dispatch_release(w->queue);
    }
    pthread_mutex_destroy(&w->lock);
    free(w);
}

sel_watch* sel_watch_open(const char* root) {
    sel_watch* w;
    CFStringRef path;
    CFArrayRef list;
    FSEventStreamContext ctx;

    if (root == NULL || strlen(root) >= PATH_MAX) {
        return NULL;
    }
    w = calloc(1, sizeof *w);
    if (w == NULL) {
        return NULL;
    }
    strcpy(w->root, root);
    if (realpath(root, w->real) == NULL) {
        free(w);
        return NULL;
    }
    pthread_mutex_init(&w->lock, NULL);
    path = CFStringCreateWithCString(NULL, w->real, kCFStringEncodingUTF8);
    list = CFArrayCreate(NULL, (const void**)&path, 1, &kCFTypeArrayCallBacks);
    memset(&ctx, 0, sizeof ctx);
    ctx.info = w;
    w->stream = FSEventStreamCreate(NULL, on_events, &ctx, list, kFSEventStreamEventIdSinceNow, 0.05,
                                    kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
    CFRelease(list);
    CFRelease(path);
    w->queue = dispatch_queue_create("selecta.watch", DISPATCH_QUEUE_SERIAL);
    if (w->stream == NULL || w->queue == NULL) {
        sel_watch_close(w);
        return NULL;
    }
    FSEventStreamSetDispatchQueue(w->stream, w->queue);
    if (!FSEventStreamStart(w->stream)) {
        sel_watch_close(w);
        return NULL;
    }
    return w;
}

uint32_t sel_watch_poll(sel_watch* w, uint32_t timeout_ms, sel_watch_dir_fn fn, void* ctx) {
    uint32_t events, i, n;
    uint64_t waited = 0;
    char(*dirs)[PATH_MAX];

    while (waited < (uint64_t)timeout_ms * 1000u) {
        int any;

        pthread_mutex_lock(&w->lock);
        any = w->events > 0;
        pthread_mutex_unlock(&w->lock);
        if (any) {
            break;
        }
        usleep(WATCH_STEP_US);
        waited += WATCH_STEP_US;
    }
    usleep(WATCH_SETTLE_US); /* let one burst of events land as one batch */
    dirs = malloc((size_t)WATCH_DIRS * PATH_MAX);
    if (dirs == NULL) {
        return 0;
    }
    pthread_mutex_lock(&w->lock);
    events = w->events;
    n = w->dir_n;
    if (w->overflow) {
        n = 1;
        strcpy(dirs[0], w->root);
    } else {
        memcpy(dirs, w->dirs, (size_t)n * PATH_MAX);
    }
    w->events = w->dir_n = 0;
    w->overflow = 0;
    pthread_mutex_unlock(&w->lock);
    for (i = 0; i < n && events > 0; i++) {
        fn(ctx, dirs[i]);
    }
    free(dirs);
    return events;
}
