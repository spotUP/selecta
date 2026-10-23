/* Modland network sync (plan Phase 4, I161a; design doc 9.1). Downloads `<base>/allmods.zip` (one member,
 * allmods.txt, lines `<size><TAB><path>`) and the changelogs of the last SEL_MODLAND_NET_DAYS days
 * (`<base>/pub/changelogs/changelog_YYYY-MM-DD.txt`; a 404 is a day without a changelog, not an error)
 * through the HTTP client into `<cache dir>/sync/modland`, then runs the file based sync of modland_sync.h
 * on that directory. Desktop only in practice: the zip is inflated from a memory buffer. */
#ifndef SELECTA_LIBRARY_SOURCES_MODLAND_NET_H
#define SELECTA_LIBRARY_SOURCES_MODLAND_NET_H

#include "modland_sync.h"

#define SEL_MODLAND_NET_DAYS 14
/* DB meta key holding the last sync's new-this-week count. */
#define SEL_MODLAND_META_NEW "modland.new_this_week"

typedef struct sel_modland_net_stats {
    uint32_t zip_bytes;          /* allmods.zip downloaded */
    uint32_t listing_bytes;      /* allmods.txt extracted */
    uint32_t changelogs_fetched; /* changelog files downloaded (HTTP 200) */
    uint32_t changelogs_missing; /* changelog days answered 404 */
    uint32_t http_requests;
} sel_modland_net_stats;

/* True for an http:// or https:// URL. */
int sel_modland_is_url(const char* s);

/* `base` is the server root (`http://ftp.modland.com`), a trailing '/' is ignored. Returns 0 when the zip cannot
 * be fetched or unpacked, a changelog fails with anything but 404, or the sync itself fails. `stats` and
 * `net` are filled either way. */
int sel_modland_net_sync(const sel_local_scan_config* cfg, const char* base, int64_t now, sel_modland_sync_stats* stats,
                         sel_modland_net_stats* net);

#endif
