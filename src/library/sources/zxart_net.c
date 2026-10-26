/* See zxart_net.h. */
#include "zxart_net.h"

#include "../cache.h"
#include "../local.h"
#include "sync_net.h"
#include "sync_util.h"

#include <selecta/engine.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u

/* Downloads the pages of `what` ("author" or "zxMusic") as files named `fmt` below `dir`. Returns the pages read,
 * 0 on a failure. A page that is not a success answer stops the sync. */
static unsigned get_pages(const sel_host* host, const char* base, size_t n, const char* dir, const char* fmt,
                          const char* what, sel_zxart_net_stats* net, int* first) {
    unsigned k;
    uint32_t total = 0;

    for (k = 0; k < SEL_ZXART_PAGES_MAX; k++) {
        char name[32], path[SEL_CACHE_PATH_MAX], url[URL_BYTES];
        unsigned long got = 0;
        FILE* f;
        int32_t items;

        if (k > 0 && (unsigned long)k * SEL_ZXART_PAGE >= total) {
            break;
        }
        snprintf(name, sizeof name, fmt, k);
        snprintf(url, sizeof url, "%.*s/api/language:eng/export:%s/start:%lu/limit:%u", (int)n, base, what,
                 (unsigned long)k * SEL_ZXART_PAGE, SEL_ZXART_PAGE);
        if (!sel_browse_join(path, sizeof path, dir, name)) {
            return 0;
        }
        if (!*first) {
            sel_sync_net_pause(host, SEL_ZXART_PAUSE_MS);
        }
        *first = 0;
        if (sel_sync_net_get_file(url, path, &net->http_requests, &got) != 200) {
            return 0;
        }
        net->list_bytes += (uint32_t)got;
        f = fopen(path, "rb");
        items = sel_zxart_read_page(f, what, NULL, NULL, &total);
        if (f != NULL) {
            fclose(f);
        }
        if (items < 0 || (items == 0 && k == 0)) {
            return 0;
        }
    }
    return k;
}

int sel_zxart_net_sync(const sel_local_scan_config* cfg, const char* base, sel_zxart_sync_stats* stats,
                       sel_zxart_net_stats* net) {
    sel_cache cache;
    char dir[SEL_CACHE_PATH_MAX];
    unsigned k;
    int first = 1;
    size_t n = strlen(base);

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 128u) {
        return 0;
    }
    sel_cache_init(&cache, NULL, 0);
    if (snprintf(dir, sizeof dir, "%s/sync/zxart", cache.dir) >= (int)sizeof dir || !sel_config_mkdir(dir)) {
        return 0;
    }
    for (k = 0; k < SEL_ZXART_PAGES_MAX; k++) { /* pages of an older sync must not outlive a shorter catalogue */
        char name[32], path[SEL_CACHE_PATH_MAX];

        snprintf(name, sizeof name, SEL_ZXART_MUSIC, k);
        if (sel_browse_join(path, sizeof path, dir, name)) {
            remove(path);
        }
        snprintf(name, sizeof name, SEL_ZXART_PEOPLE, k);
        if (sel_browse_join(path, sizeof path, dir, name)) {
            remove(path);
        }
    }
    if (get_pages(cfg->host, base, n, dir, SEL_ZXART_PEOPLE, "author", net, &first) == 0 ||
        get_pages(cfg->host, base, n, dir, SEL_ZXART_MUSIC, "zxMusic", net, &first) == 0) {
        return 0;
    }
    return sel_zxart_sync(cfg, dir, stats);
}
