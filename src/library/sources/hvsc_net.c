/* See hvsc_net.h. */
#include "hvsc_net.h"

#include "../cache.h"
#include "../local.h"
#include "sync_net.h"
#include "sync_util.h"

#include <selecta/engine.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define LINE_BYTES 2400u

/* Writes the `; /PATH` lines of Songlengths.md5 as `/PATH` lines: the catalogue listing. Returns the count, or
 * -1 when a file cannot be opened or written. */
static int64_t derive_listing(const char* songlengths, const char* listing) {
    FILE* in = fopen(songlengths, "rb");
    FILE* out;
    char line[LINE_BYTES];
    int64_t n = 0;

    if (in == NULL) {
        return -1;
    }
    out = fopen(listing, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while (fgets(line, sizeof line, in) != NULL) {
        sel_sync_chomp(line);
        if (line[0] == ';' && line[1] == ' ' && line[2] == '/') {
            if (fprintf(out, "%s\n", line + 2) < 0) {
                n = -1;
                break;
            }
            n++;
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        return -1;
    }
    return n;
}

int sel_hvsc_net_sync(const sel_local_scan_config* cfg, const char* base, sel_hvsc_sync_stats* stats,
                      sel_hvsc_net_stats* net) {
    sel_cache cache;
    char dir[SEL_CACHE_PATH_MAX];
    char songlengths[SEL_CACHE_PATH_MAX];
    char stil[SEL_CACHE_PATH_MAX];
    char listing[SEL_CACHE_PATH_MAX];
    char url[URL_BYTES];
    unsigned long got = 0;
    size_t n = strlen(base);
    int64_t derived;

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 64u) {
        return 0;
    }
    sel_cache_init(&cache, NULL, 0);
    if (snprintf(dir, sizeof dir, "%s/sync/hvsc", cache.dir) >= (int)sizeof dir || !sel_config_mkdir(dir) ||
        !sel_browse_join(songlengths, sizeof songlengths, dir, SEL_HVSC_SONGLENGTHS) ||
        !sel_browse_join(stil, sizeof stil, dir, SEL_HVSC_STIL) ||
        !sel_browse_join(listing, sizeof listing, dir, SEL_HVSC_LISTING)) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s" SEL_HVSC_DOC_DIR SEL_HVSC_SONGLENGTHS, (int)n, base);
    if (sel_sync_net_get_file(url, songlengths, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->songlengths_bytes = (uint32_t)got;
    snprintf(url, sizeof url, "%.*s" SEL_HVSC_DOC_DIR SEL_HVSC_STIL, (int)n, base);
    if (sel_sync_net_get_file(url, stil, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->stil_bytes = (uint32_t)got;
    derived = derive_listing(songlengths, listing);
    if (derived < 1) {
        return 0;
    }
    net->listing_derived = (uint32_t)derived;
    return sel_hvsc_sync(cfg, dir, stats);
}
