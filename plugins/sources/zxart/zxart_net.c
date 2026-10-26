/* See zxart_net.h. */
#include "zxart_net.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define PATH_BYTES 1100u

/* Downloads the pages of `what` ("author" or "zxMusic") as files named `fmt` below `dir`. Returns the pages read,
 * 0 on a failure. A page that is not a success answer stops the sync. */
static unsigned get_pages(const sel_host* host, const sel_net* net_svc, const char* base, size_t n, const char* dir, const char* fmt,
                          const char* what, sel_zxart_net_stats* net, int* first) {
    unsigned k;
    uint32_t total = 0;

    for (k = 0; k < SEL_ZXART_PAGES_MAX; k++) {
        char name[32], path[PATH_BYTES], url[URL_BYTES];
        uint32_t requests = 0;
        unsigned long got = 0;
        FILE* f;
        int32_t items;

        if (k > 0 && (unsigned long)k * SEL_ZXART_PAGE >= total) {
            break;
        }
        snprintf(name, sizeof name, fmt, k);
        snprintf(url, sizeof url, "%.*s/api/language:eng/export:%s/start:%lu/limit:%u", (int)n, base, what,
                 (unsigned long)k * SEL_ZXART_PAGE, SEL_ZXART_PAGE);
        if (snprintf(path, sizeof path, "%s/%s", dir, name) >= (int)sizeof path) {
            return 0;
        }
        if (!*first) {
            net_svc->pause(host, SEL_ZXART_PAUSE_MS);
        }
        *first = 0;
        if (net_svc->get_file(host, url, path, &requests, &got) != 200) {
            net->http_requests += requests;
            return 0;
        }
        net->http_requests += requests;
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

int sel_zxart_net_sync(const sel_host* host, const char* base, sel_zxart_sync_stats* stats,
                       sel_zxart_net_stats* net) {
    const sel_net* net_svc = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[PATH_BYTES - 64u];
    unsigned k;
    int first = 1;
    size_t n = strlen(base);

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (net_svc == NULL || cat == NULL || n == 0 || n >= URL_BYTES - 128u ||
        !cat->cache_dir(host, "zxart", dir, sizeof dir)) {
        return 0;
    }
    for (k = 0; k < SEL_ZXART_PAGES_MAX; k++) { /* pages of an older sync must not outlive a shorter catalogue */
        char name[32], path[PATH_BYTES];

        snprintf(name, sizeof name, SEL_ZXART_MUSIC, k);
        if (snprintf(path, sizeof path, "%s/%s", dir, name) < (int)sizeof path) {
            remove(path);
        }
        snprintf(name, sizeof name, SEL_ZXART_PEOPLE, k);
        if (snprintf(path, sizeof path, "%s/%s", dir, name) < (int)sizeof path) {
            remove(path);
        }
    }
    if (get_pages(host, net_svc, base, n, dir, SEL_ZXART_PEOPLE, "author", net, &first) == 0 ||
        get_pages(host, net_svc, base, n, dir, SEL_ZXART_MUSIC, "zxMusic", net, &first) == 0) {
        return 0;
    }
    return sel_zxart_sync(host, dir, stats);
}
