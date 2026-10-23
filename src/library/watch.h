/* Folder watcher (plan M3 Phase 4, I160; design doc F16). One implementation per desktop OS under
 * src/core/<os>/watch.c (FSEvents on macOS, inotify on Linux); the Amiga build has no watcher and
 * sel_watch_open answers NULL there, the caller falls back to the mtime poll in sources/local.c. */
#ifndef SELECTA_LIBRARY_WATCH_H
#define SELECTA_LIBRARY_WATCH_H

#include <stdint.h>

typedef struct sel_watch sel_watch;

/* Called once per changed directory (deduplicated within one poll), path spelled under the root
 * the watcher was opened with. */
typedef void (*sel_watch_dir_fn)(void* ctx, const char* dir);

/* Starts watching `root` and everything below it. NULL when watching is unavailable. */
sel_watch* sel_watch_open(const char* root);

/* Waits up to `timeout_ms` for a change. When one arrives it waits a short settle time so one burst
 * is one batch, then calls `fn` per changed directory. Returns the number of raw events seen (0 on
 * timeout). The wait is bounded: the call never blocks longer than timeout_ms plus the settle time. */
uint32_t sel_watch_poll(sel_watch* w, uint32_t timeout_ms, sel_watch_dir_fn fn, void* ctx);

void sel_watch_close(sel_watch* w);

#endif /* SELECTA_LIBRARY_WATCH_H */
