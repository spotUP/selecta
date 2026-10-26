/* Helpers shared by the source syncs (modland_sync.c, hvsc_sync.c, hippo_sync.c). */
#ifndef SELECTA_LIBRARY_SOURCES_SYNC_UTIL_H
#define SELECTA_LIBRARY_SOURCES_SYNC_UTIL_H

#include "../local.h"
#include "local_source.h"

#include <string.h>

/* Strips trailing line ends and spaces in place. */
static inline void sel_sync_chomp(char* s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
        s[--n] = '\0';
    }
}

/* Is `p` a path inside a source (not empty, not absolute, no backslash, no "..")? A song's path can become a file's
 * place on this computer, so a server's list line that names "../x" or "/x" is not taken. */
static inline int sel_sync_safe_path(const char* p) {
    return p[0] != '\0' && p[0] != '/' && strchr(p, '\\') == NULL && strstr(p, "..") == NULL;
}

/* Sends one library request; 0 when it fails or the worker answers a nonzero status. */
static inline int sel_sync_request(const sel_local_scan_config* cfg, uint32_t req, const void* payload) {
    sel_message reply;

    memset(&reply, 0, sizeof reply);
    return sel_library_request(cfg->lib, req, payload, &reply) && reply.args[1].v.i == 0;
}

/* Writes the `*n` queued rows as one SEL_LIB_REQ_SCAN_BATCH and resets `*n`. Counts the batch in `*batches`;
 * a rejected batch sets `*failed` and returns 0. Nothing queued is a success. */
static inline int sel_sync_flush_scan(const sel_local_scan_config* cfg, sel_scan_row* rows, uint32_t* n,
                                      uint32_t* batches, int* failed) {
    sel_scan_batch b;

    if (*n == 0) {
        return 1;
    }
    b.rows = rows;
    b.count = *n;
    b.now = cfg->now;
    *n = 0;
    if (!sel_sync_request(cfg, SEL_LIB_REQ_SCAN_BATCH, &b)) {
        *failed = 1;
        return 0;
    }
    (*batches)++;
    return 1;
}

#endif
