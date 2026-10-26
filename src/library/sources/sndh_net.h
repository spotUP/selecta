/* SNDH network sync (row I414). Finds the newest yearly archive on `<base>/download.php`, reads its central
 * directory with a HEAD and two Range requests into `<cache dir>/sync/sndh/sndh.cd` and runs the file based sync of
 * sndh_sync.h on that directory. Verified host: base `https://sndh.atari.org` (2026-10-26: newest archive
 * sndh2026_lf.zip, 105,124,396 bytes, directory 696,049 bytes, 6,657 records); the `sndh` entry of the mirror list in
 * net/mirrors.c serves the tunes below `<base>/sndh/sndh_lf/`. */
#ifndef SELECTA_LIBRARY_SOURCES_SNDH_NET_H
#define SELECTA_LIBRARY_SOURCES_SNDH_NET_H

#include "sndh_sync.h"

typedef struct sel_sndh_net_stats {
    uint32_t list_bytes; /* the directory bytes downloaded */
    uint32_t http_requests;
} sel_sndh_net_stats;

/* `base` is the site root (a trailing '/' is ignored). Returns 0 when the page, the archive or its directory cannot
 * be read, or the sync fails. `stats` and `net` are filled either way. */
int sel_sndh_net_sync(const sel_local_scan_config* cfg, const char* base, sel_sndh_sync_stats* stats,
                      sel_sndh_net_stats* net);

#endif
