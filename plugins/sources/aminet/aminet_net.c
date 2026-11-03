/* See aminet_net.h. */
#include "aminet_net.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>
#include "source/url_path.h"
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define PATH_BYTES 1100u

int sel_aminet_net_sync(const sel_host* host, const char* base, sel_aminet_sync_stats* stats,
                        sel_aminet_net_stats* net) {
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
        !cat->cache_dir(host, "aminet", dir, sizeof dir) ||
        snprintf(list, sizeof list, "%s/%s", dir, SEL_AMINET_LIST) >= (int)sizeof list) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s/" SEL_AMINET_LIST, (int)n, base);
    if (net_svc->get_file(host, url, list, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->list_bytes = (uint32_t)got;
    return sel_aminet_sync(host, dir, stats);
}

int sel_aminet_net_search(const sel_host* host, const char* base, const char* query, const sel_search_sink* out) {
    const sel_net* net_svc = host != NULL && host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host != NULL && host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[PATH_BYTES - 64u], path[PATH_BYTES], url[URL_BYTES], enc[300];
    uint32_t requests = 0;
    unsigned long got = 0;
    size_t n;
    int32_t rows = -1;

    if (net_svc == NULL || cat == NULL || base == NULL || query == NULL || out == NULL ||
        !cat->cache_dir(host, "aminet", dir, sizeof dir) || !sel_url_query_encode(query, enc, sizeof enc)) {
        return -1;
    }
    n = strlen(base);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 400u) {
        return -1;
    }
    snprintf(url, sizeof url, "%.*s" SEL_AMINET_SEARCH_PATH, (int)n, base, enc);
    snprintf(path, sizeof path, "%s/search.html", dir);
    if (net_svc->get_file(host, url, path, &requests, &got) == 200) {
        rows = sel_aminet_search_file(host, path, out);
    }
    remove(path);
    return (int)rows;
}
