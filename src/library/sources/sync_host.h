/* What a built-in source's `sync` finds behind its sel_host (I517 phase 1). The engine binds the host per sync call:
 * `host->ctx` points at a sel_sync_ctx. Phases 3 and 4 move the sources out of core; they then talk to the host's
 * net and catalogue services and this file goes with the last wrapper. */
#ifndef SELECTA_LIBRARY_SOURCES_SYNC_HOST_H
#define SELECTA_LIBRARY_SOURCES_SYNC_HOST_H

#include "local_source.h"

#include <selecta/source.h>

struct sel_cat_batch;

typedef struct sel_sync_ctx {
    sel_local_scan_config scfg;
    struct sel_cat_batch* batch; /* open catalogue batch, owned by sync_services.c */
} sel_sync_ctx;

/* Fills `out` as `base` with ctx = `ctx` and the net and catalogue accessors set. The engine calls it per sync. */
void sel_sync_host_bind(sel_host* out, const sel_host* base, sel_sync_ctx* ctx);

/* Frees a batch a source left open (an aborted or crashed sync). */
void sel_sync_ctx_release(sel_sync_ctx* ctx);

static inline const sel_local_scan_config* sel_sync_cfg(const sel_host* host) {
    return &((const sel_sync_ctx*)host->ctx)->scfg;
}

/* Appends one counter to a stats list that starts with SEL_TAG_END at [0]. A list that is full drops the counter. */
static inline void sel_sync_stat(sel_tag* stats, uint32_t cap, uint32_t tag, uintptr_t value) {
    uint32_t n = 0;

    while (stats[n].tag != SEL_TAG_END) {
        n++;
    }
    if (n + 2u > cap) {
        return;
    }
    stats[n].tag = tag;
    stats[n].value = value;
    stats[n + 1u].tag = SEL_TAG_END;
    stats[n + 1u].value = 0;
}

#endif
