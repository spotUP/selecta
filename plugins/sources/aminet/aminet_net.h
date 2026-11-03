/* aminet network sync (row I405). Downloads `<base>/INDEX` through the HTTP client into
 * `<cache dir>/sync/aminet` and runs the file based sync of aminet_sync.h on that directory. Verified host: base
 * `https://aminet.net` (HTTP 200, 7,252,689 bytes, 85,712 lines, 2026-10-30); the `aminet` entry of the mirror
 * list in net/mirrors.c serves the archives below `<base>/`. */
#ifndef SELECTA_PLUGINS_SOURCES_AMINET_NET_H
#define SELECTA_PLUGINS_SOURCES_AMINET_NET_H

#include "aminet_sync.h"

typedef struct sel_aminet_net_stats {
    uint32_t list_bytes; /* INDEX downloaded */
    uint32_t http_requests;
} sel_aminet_net_stats;

/* `base` is the site root (a trailing '/' is ignored). Returns 0 when the list cannot be fetched or the sync
 * fails. `stats` and `net` are filled either way. */
int sel_aminet_net_sync(const sel_host* host, const char* base, sel_aminet_sync_stats* stats,
                        sel_aminet_net_stats* net);

#define SEL_AMINET_SEARCH_PATH "/search?query=%s&path%%5B%%5D=mods&type=advanced&sort=name"

/* I1024g: one GET `<base>/search?query=<query>&path[]=mods&type=advanced&sort=name`; the packages go to `out` as the
 * sync's rows and the catalogue is not touched. The answer file is the search's own and is removed. Returns the rows
 * (0 for no hit), -1 when the site answers no 200. */
int sel_aminet_net_search(const sel_host* host, const char* base, const char* query, const sel_search_sink* out);

#endif
