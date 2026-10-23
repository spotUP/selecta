/* See modland_net.h. */
#include "modland_net.h"

#include "../cache.h"
#include "../local.h"
#include "../net/http.h"
#include "archive.h"
#include "sync_net.h"

#include <selecta/engine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL_BYTES 1100u
#define ZIP_NAME "allmods.zip"
#define MEMBER_NAME "allmods.txt"
#define CHANGELOG_DIR "/pub/changelogs/"
#define MAX_LISTING_BYTES ((size_t)512u * 1024u * 1024u)

int sel_modland_is_url(const char* s) {
    return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

/* Streams the zip's allmods.txt member to `out_path` (I161c): no whole-archive or whole-member buffer. */
static int unpack_listing(const sel_host* host, const char* zip_path, const char* out_path, sel_modland_net_stats* net) {
    return sel_archive_extract_file(host, zip_path, MEMBER_NAME, MAX_LISTING_BYTES, out_path, &net->listing_bytes,
                                    &net->unpack_peak_heap);
}

/* Civil date of a day number (days since 1970-01-01); integers only. */
static void civil_from_days(int64_t z, int* y, int* m, int* d) {
    int64_t era, doe, yoe, doy, mp, yy;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yy = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}

/* Deletes changelogs left in `dir` by an earlier sync so only the days fetched now are applied. */
static void remove_stale_changelogs(const sel_host* host, const char* dir) {
    sel_browse br;
    char path[SEL_BROWSE_PATH_MAX];
    uint32_t i;

    if (!sel_browse_init(&br, host, 512u, 16384u)) {
        return;
    }
    if (sel_browse_list(&br, dir)) {
        for (i = 0; i < br.count; i++) {
            const char* name = sel_browse_name(&br, i);

            if (strncmp(name, SEL_MODLAND_CHANGELOG_PREFIX, sizeof SEL_MODLAND_CHANGELOG_PREFIX - 1u) == 0 &&
                sel_browse_join(path, sizeof path, dir, name)) {
                remove(path);
            }
        }
    }
    sel_browse_free(&br, host);
}

int sel_modland_net_sync(const sel_local_scan_config* cfg, const char* base, int64_t now, sel_modland_sync_stats* stats,
                         sel_modland_net_stats* net) {
    sel_cache cache;
    char dir[SEL_CACHE_PATH_MAX];
    char zip[SEL_CACHE_PATH_MAX];
    char listing[SEL_CACHE_PATH_MAX];
    char file[SEL_CACHE_PATH_MAX];
    char url[URL_BYTES];
    char root[URL_BYTES];
    unsigned long got = 0;
    size_t n = strlen(base);
    int day;

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= sizeof root) {
        return 0;
    }
    memcpy(root, base, n);
    root[n] = '\0';
    sel_cache_init(&cache, NULL, 0);
    if (snprintf(dir, sizeof dir, "%s/sync/modland", cache.dir) >= (int)sizeof dir || !sel_config_mkdir(dir) ||
        !sel_browse_join(zip, sizeof zip, dir, ZIP_NAME) ||
        !sel_browse_join(listing, sizeof listing, dir, SEL_MODLAND_LISTING)) {
        return 0;
    }
    remove_stale_changelogs(cfg->host, dir);
    snprintf(url, sizeof url, "%s/" ZIP_NAME, root);
    if (sel_sync_net_get_file(url, zip, &net->http_requests, &got) != 200) {
        return 0;
    }
    net->zip_bytes = (uint32_t)got;
    if (!unpack_listing(cfg->host, zip, listing, net)) {
        return 0;
    }
    for (day = 0; day < SEL_MODLAND_NET_DAYS; day++) {
        char name[48];
        int y, m, d, status;

        civil_from_days(now / 86400 - day, &y, &m, &d);
        snprintf(name, sizeof name, SEL_MODLAND_CHANGELOG_PREFIX "%04d-%02d-%02d.txt", y, m, d);
        snprintf(url, sizeof url, "%s" CHANGELOG_DIR "%s", root, name);
        if (!sel_browse_join(file, sizeof file, dir, name)) {
            return 0;
        }
        status = sel_sync_net_get_file(url, file, &net->http_requests, &got);
        if (status == 200) {
            net->changelogs_fetched++;
        } else if (status == 404) {
            net->changelogs_missing++;
        } else {
            return 0;
        }
    }
    return sel_modland_sync(cfg, dir, now, stats);
}
