/* See modland_net.h. */
#include "modland_net.h"

#include "../cache.h"
#include "../local.h"
#include "../net/http.h"
#include "archive.h"

#include <selecta/engine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define URL_BYTES 1100u
#define ZIP_NAME "allmods.zip"
#define MEMBER_NAME "allmods.txt"
#define CHANGELOG_DIR "/pub/changelogs/"
#define MAX_LISTING_BYTES ((size_t)512u * 1024u * 1024u)

typedef struct net_ctx {
    FILE* f;
    unsigned long bytes;
} net_ctx;

typedef struct member_ctx {
    const char* out_path;
    uint32_t bytes;
    int found;
    int ok;
} member_ctx;

int sel_modland_is_url(const char* s) {
    return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

static int file_sink(void* ctx, const void* data, size_t len) {
    net_ctx* c = ctx;

    c->bytes += (unsigned long)len;
    return fwrite(data, 1, len, c->f) == len;
}

/* GETs `url` into `path`. Returns the HTTP status (0 on a transport failure). Any result but 200 removes the file. */
static int get_file(const char* url, const char* path, sel_modland_net_stats* net, unsigned long* bytes) {
    sel_http_req req;
    sel_http_res res;
    net_ctx c;
    sel_http_err err;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    c.bytes = 0;
    c.f = fopen(path, "wb");
    if (c.f == NULL) {
        return 0;
    }
    req.url = url;
    err = sel_http_request(&req, file_sink, &c, &res);
    net->http_requests += res.requests;
    fclose(c.f);
    *bytes = c.bytes;
    if (err != SEL_HTTP_OK || res.status != 200) {
        remove(path);
    }
    return err == SEL_HTTP_OK ? res.status : 0;
}

static int member_sink(void* ctx, const char* name, const uint8_t* data, size_t size) {
    member_ctx* m = ctx;
    const char* base = strrchr(name, '/');
    FILE* f;

    base = base != NULL ? base + 1 : name;
    if (strcmp(base, MEMBER_NAME) != 0) {
        return 1;
    }
    m->found = 1;
    f = fopen(m->out_path, "wb");
    if (f == NULL) {
        return 0;
    }
    m->ok = fwrite(data, 1, size, f) == size;
    m->bytes = (uint32_t)size;
    fclose(f);
    return 0;
}

/* Reads the zip into one host buffer and writes its allmods.txt member to `out_path`. */
static int unpack_listing(const sel_host* host, const char* zip_path, const char* out_path, uint32_t* bytes) {
    FILE* f = fopen(zip_path, "rb");
    uint8_t* buf;
    long size;
    member_ctx m;
    int kind;

    if (f == NULL) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = size > 0 ? host->mem_alloc(host, (size_t)size, SEL_MEM_FAST) : NULL;
    if (buf == NULL || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        if (buf != NULL) {
            host->mem_free(host, buf);
        }
        return 0;
    }
    fclose(f);
    memset(&m, 0, sizeof m);
    m.out_path = out_path;
    kind = sel_archive_kind(buf, (size_t)size);
    if (kind == SEL_ARCHIVE_ZIP) {
        sel_archive_each(host, kind, buf, (size_t)size, MAX_LISTING_BYTES, member_sink, &m);
    }
    host->mem_free(host, buf);
    *bytes = m.bytes;
    return m.found && m.ok;
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
    if (get_file(url, zip, net, &got) != 200) {
        return 0;
    }
    net->zip_bytes = (uint32_t)got;
    if (!unpack_listing(cfg->host, zip, listing, &net->listing_bytes)) {
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
        status = get_file(url, file, net, &got);
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
