/* See keygenmusic_net.h. */
#include "keygenmusic_net.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define PATH_BYTES 1100u

int sel_keygen_net_sync(const sel_host* host, const char* base, sel_keygen_sync_stats* stats,
                        sel_keygen_net_stats* net) {
    const sel_net* net_svc = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[PATH_BYTES - 64u];
    char list[PATH_BYTES];
    char url[URL_BYTES];
    unsigned long got = 0;
    size_t n = strlen(base);

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (net_svc == NULL || cat == NULL || n == 0 || n >= URL_BYTES - 64u ||
        !cat->cache_dir(host, "keygenmusic", dir, sizeof dir) ||
        snprintf(list, sizeof list, "%s/%s", dir, SEL_KEYGEN_LIST) >= (int)sizeof list) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s/" SEL_KEYGEN_ROOT SEL_KEYGEN_LIST, (int)n, base);
    if (net_svc->get_file(host, url, list, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->list_bytes = (uint32_t)got;
    return sel_keygen_sync(host, dir, stats);
}
