/* keygenmusic network sync (row I416). Downloads `<base>/kgm/lib.txt` through the HTTP client into
 * `<cache dir>/sync/keygenmusic` and runs the file based sync of keygenmusic_sync.h on that directory. Verified host:
 * base `https://keygenmusic.tk` (HTTP 200, 790 KB, 4,350 songs, 2026-10-26); the `keygenmusic` entry of the mirror
 * list in net/mirrors.c serves the songs below `<base>/kgm/`. */
#ifndef SELECTA_LIBRARY_SOURCES_KEYGENMUSIC_NET_H
#define SELECTA_LIBRARY_SOURCES_KEYGENMUSIC_NET_H

#include "keygenmusic_sync.h"

typedef struct sel_keygen_net_stats {
    uint32_t list_bytes; /* lib.txt downloaded */
    uint32_t http_requests;
} sel_keygen_net_stats;

/* `base` is the site root (a trailing '/' is ignored). Returns 0 when the list cannot be fetched or the sync
 * fails. `stats` and `net` are filled either way. */
int sel_keygen_net_sync(const sel_local_scan_config* cfg, const char* base, sel_keygen_sync_stats* stats,
                        sel_keygen_net_stats* net);

#endif
