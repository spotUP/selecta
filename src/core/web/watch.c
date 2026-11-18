/* Web: no folder watcher (plan 2026-10-29-web-port-plan.md section 2, I694a/I694b). Declared in src/library/watch.h; the browser has
 * no inotify and OPFS has no change events. Callers poll directory modification times (sel_local_poll_changed). */
#include "../../library/watch.h"

#include <stddef.h>

sel_watch* sel_watch_open(const char* root) {
    (void)root;
    return NULL;
}

uint32_t sel_watch_poll(sel_watch* w, uint32_t timeout_ms, sel_watch_dir_fn fn, void* ctx) {
    (void)w;
    (void)timeout_ms;
    (void)fn;
    (void)ctx;
    return 0;
}

void sel_watch_close(sel_watch* w) {
    (void)w;
}
