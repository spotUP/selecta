/* Linux folder watcher on inotify. Declared in src/library/watch.h. inotify is not recursive, so
 * every directory under the root gets its own watch (added on open, and for each directory created
 * later). Runs on the pump thread only. */
#include "../../library/watch.h"

#include <dirent.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#define WATCH_MAX 512u
#define WATCH_DIRS 64u
#define WATCH_SETTLE_MS 150

struct sel_watch {
    int fd;
    int wd[WATCH_MAX];
    char* path[WATCH_MAX];
    uint32_t count;
    char dirs[WATCH_DIRS][PATH_MAX];
    uint32_t dir_n, events;
    int overflow;
};

static const uint32_t g_mask = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;

static void add_tree(sel_watch* w, const char* dir) {
    DIR* d;
    struct dirent* de;
    int wd;

    if (w->count >= WATCH_MAX) {
        return;
    }
    wd = inotify_add_watch(w->fd, dir, g_mask);
    if (wd < 0) {
        return;
    }
    w->wd[w->count] = wd;
    w->path[w->count] = strdup(dir);
    w->count++;
    d = opendir(dir);
    if (d == NULL) {
        return;
    }
    while ((de = readdir(d)) != NULL) {
        char full[PATH_MAX];
        struct stat st;

        if (de->d_name[0] == '.') {
            continue;
        }
        if ((size_t)snprintf(full, sizeof full, "%s/%s", dir, de->d_name) < sizeof full && stat(full, &st) == 0 &&
            S_ISDIR(st.st_mode)) {
            add_tree(w, full);
        }
    }
    closedir(d);
}

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

sel_watch* sel_watch_open(const char* root) {
    sel_watch* w = calloc(1, sizeof *w);

    if (w == NULL) {
        return NULL;
    }
    w->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (w->fd < 0) {
        free(w);
        return NULL;
    }
    add_tree(w, root);
    if (w->count == 0) {
        sel_watch_close(w);
        return NULL;
    }
    return w;
}

/* Reads and files every queued event. */
static void drain(sel_watch* w) {
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t n;

    while ((n = read(w->fd, buf, sizeof buf)) > 0) {
        char* p = buf;

        while (p < buf + n) {
            const struct inotify_event* ev = (const struct inotify_event*)p;
            uint32_t i;

            for (i = 0; i < w->count; i++) {
                if (w->wd[i] == ev->wd) {
                    w->events++;
                    note_dir(w, w->path[i]);
                    if ((ev->mask & IN_CREATE) && (ev->mask & IN_ISDIR) && ev->len > 0) {
                        char full[PATH_MAX];

                        if ((size_t)snprintf(full, sizeof full, "%s/%s", w->path[i], ev->name) < sizeof full) {
                            add_tree(w, full);
                        }
                    }
                    break;
                }
            }
            p += sizeof *ev + ev->len;
        }
    }
}

uint32_t sel_watch_poll(sel_watch* w, uint32_t timeout_ms, sel_watch_dir_fn fn, void* ctx) {
    struct pollfd pfd;
    uint32_t events, i;

    pfd.fd = w->fd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, (int)timeout_ms) > 0) {
        poll(&pfd, 1, WATCH_SETTLE_MS); /* let one burst land as one batch */
        drain(w);
    }
    events = w->events;
    if (w->overflow) {
        fn(ctx, w->path[0]);
    } else {
        for (i = 0; i < w->dir_n; i++) {
            fn(ctx, w->dirs[i]);
        }
    }
    w->events = w->dir_n = 0;
    w->overflow = 0;
    return events;
}

void sel_watch_close(sel_watch* w) {
    uint32_t i;

    if (w == NULL) {
        return;
    }
    for (i = 0; i < w->count; i++) {
        free(w->path[i]);
    }
    close(w->fd);
    free(w);
}
