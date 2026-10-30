/* HVSC network sync (plan Phase 4, I162a; design doc 9.1). Downloads
 * `<base>/C64Music/DOCUMENTS/Songlengths.md5` and `<base>/C64Music/DOCUMENTS/STIL.txt` through the HTTP client
 * into `<cache dir>/sync/hvsc`, derives hvsc_listing.txt from the `; /PATH` comment lines of Songlengths.md5
 * (HVSC publishes no separate file listing), then runs the file based sync of hvsc_sync.h on that directory.
 * Base `https://hvsc.de/download/` (hvsc.c64.org/download redirects there; the c64.org site root answers 200
 * with an HTML page, which the content checks refuse and delete, I718); the `hvsc` entry of the mirror list in
 * net/mirrors.c and the source's home carry the same base. */
#ifndef SELECTA_PLUGINS_SOURCES_HVSC_NET_H
#define SELECTA_PLUGINS_SOURCES_HVSC_NET_H

#include "hvsc_sync.h"

#define SEL_HVSC_DOC_DIR "/C64Music/DOCUMENTS/"

typedef struct sel_hvsc_net_stats {
    uint32_t songlengths_bytes; /* Songlengths.md5 downloaded */
    uint32_t stil_bytes;        /* STIL.txt downloaded */
    uint32_t listing_derived;   /* listing lines written from Songlengths.md5 */
    uint32_t http_requests;
} sel_hvsc_net_stats;

/* `base` is the server root holding C64Music/ (a trailing '/' is ignored). Returns 0 when a file cannot be
 * fetched, the listing cannot be derived, or the sync fails. `stats` and `net` are filled either way. */
int sel_hvsc_net_sync(const sel_host* host, const char* base, sel_hvsc_sync_stats* stats, sel_hvsc_net_stats* net);

#endif
