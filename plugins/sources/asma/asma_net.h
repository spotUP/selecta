/* ASMA network sync (row I413). Downloads `<base>/asmadb/asmadb.js` through the HTTP client into
 * `<cache dir>/sync/asma` and runs the file based sync of asma_sync.h on that directory. Verified host: base
 * `https://asma.atari.org` (HTTP 200, 2.2 MB, 2026-10-26); the `asma` entry of the mirror list in net/mirrors.c
 * serves the songs below `<base>/asma/`. */
#ifndef SELECTA_PLUGINS_SOURCES_ASMA_NET_H
#define SELECTA_PLUGINS_SOURCES_ASMA_NET_H

#include "asma_sync.h"

typedef struct sel_asma_net_stats {
    uint32_t list_bytes; /* asmadb.js downloaded */
    uint32_t http_requests;
} sel_asma_net_stats;

/* `base` is the site root (a trailing '/' is ignored). Returns 0 when the list cannot be fetched or the sync
 * fails. `stats` and `net` are filled either way. */
int sel_asma_net_sync(const sel_host* host, const char* base, sel_asma_sync_stats* stats,
                      sel_asma_net_stats* net);

#endif
