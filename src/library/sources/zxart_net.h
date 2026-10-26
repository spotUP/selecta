/* ZX-Art network sync (row I415). Downloads the pages of `<base>/api/language:eng/export:{author,zxMusic}/start:N/limit:1000`
 * into `<cache dir>/sync/zxart/` with a pause of one second between requests (bbmp: the catalogue is read gently) and
 * runs the file based sync of zxart_sync.h on that directory. Verified host: base `https://zxart.ee` (2026-10-26:
 * 29,731 tunes, 19,658 people, 30 + 20 pages). robots.txt keeps crawlers from /api; a player that reads the
 * catalogue when its user asks for a sync is no crawler (bbmp zxart.py says the same). */
#ifndef SELECTA_LIBRARY_SOURCES_ZXART_NET_H
#define SELECTA_LIBRARY_SOURCES_ZXART_NET_H

#include "zxart_sync.h"

#define SEL_ZXART_PAUSE_MS 1000u

typedef struct sel_zxart_net_stats {
    uint32_t list_bytes; /* the page bytes downloaded */
    uint32_t http_requests;
} sel_zxart_net_stats;

/* `base` is the site root (a trailing '/' is ignored). Returns 0 when a page cannot be read or is not a success
 * answer, or the sync fails. `stats` and `net` are filled either way. */
int sel_zxart_net_sync(const sel_local_scan_config* cfg, const char* base, sel_zxart_sync_stats* stats,
                       sel_zxart_net_stats* net);

#endif
