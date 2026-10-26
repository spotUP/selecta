/* See keygenmusic_net.h. */
#include "keygenmusic_net.h"

#include "../cache.h"
#include "../local.h"
#include "sync_net.h"
#include "sync_util.h"

#include <selecta/engine.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u

int sel_keygen_net_sync(const sel_local_scan_config* cfg, const char* base, sel_keygen_sync_stats* stats,
                        sel_keygen_net_stats* net) {
    sel_cache cache;
    char dir[SEL_CACHE_PATH_MAX];
    char list[SEL_CACHE_PATH_MAX];
    char url[URL_BYTES];
    unsigned long got = 0;
    size_t n = strlen(base);

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 64u) {
        return 0;
    }
    sel_cache_init(&cache, NULL, 0);
    if (snprintf(dir, sizeof dir, "%s/sync/keygenmusic", cache.dir) >= (int)sizeof dir || !sel_config_mkdir(dir) ||
        !sel_browse_join(list, sizeof list, dir, SEL_KEYGEN_LIST)) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s/" SEL_KEYGEN_ROOT SEL_KEYGEN_LIST, (int)n, base);
    if (sel_sync_net_get_file(url, list, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->list_bytes = (uint32_t)got;
    return sel_keygen_sync(cfg, dir, stats);
}
