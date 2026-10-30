/* HippoPlayer network sync (I718; design doc 9.1 and 17). Downloads `<base>/api/v1/playlists` (JSON list) and
 * `<base>/uhcplaylists.php` (TSV list) into `<cache dir>/sync/hippo` as hippo_list.json and uhcplaylists.tsv through
 * the host net service, content-checked (JSON opens with '[', the TSV has tab separated lines; a refused body is
 * deleted), then runs sel_hippo_sync on that directory. Modelled on sel_hvsc_net_sync. The source's home
 * `https://hippoplayer.se/` is the default base the desktop Sync click sends. */
#ifndef SELECTA_LIBRARY_SOURCES_HIPPO_NET_H
#define SELECTA_LIBRARY_SOURCES_HIPPO_NET_H

#include "hippo_sync.h"

#include <selecta/host.h>

#define SEL_HIPPO_API_PATH "api/v1/playlists"
#define SEL_HIPPO_TSV_PATH "uhcplaylists.php"

typedef struct sel_hippo_net_stats {
    uint32_t json_bytes; /* JSON list downloaded and accepted */
    uint32_t tsv_bytes;  /* TSV list downloaded and accepted */
    uint32_t http_requests;
} sel_hippo_net_stats;

/* `base` is the server root (a trailing '/' is ignored). Returns 0 when neither list could be fetched and
 * accepted, or the sync fails. `stats` and `net` are filled either way. */
int sel_hippo_net_sync(const sel_host* host, const char* base, sel_hippo_sync_stats* stats, sel_hippo_net_stats* net);

#endif
