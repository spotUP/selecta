/* See sndh_net.h. */
#include "sndh_net.h"

#include "../cache.h"
#include "../local.h"
#include "sync_net.h"
#include "sync_util.h"

#include <selecta/engine.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u

/* Reads at most `cap` bytes of `file` into `buf`. */
static size_t slurp(const char* file, unsigned char* buf, size_t cap) {
    FILE* f = fopen(file, "rb");
    size_t n;

    if (f == NULL) {
        return 0;
    }
    n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

int sel_sndh_net_sync(const sel_local_scan_config* cfg, const char* base, sel_sndh_sync_stats* stats,
                      sel_sndh_net_stats* net) {
    sel_cache cache;
    char dir[SEL_CACHE_PATH_MAX];
    char page[SEL_CACHE_PATH_MAX];
    char tail[SEL_CACHE_PATH_MAX];
    char cd[SEL_CACHE_PATH_MAX];
    char url[URL_BYTES];
    char zip[URL_BYTES];
    unsigned long got = 0, size = 0, from;
    unsigned year;
    uint32_t cd_offset, cd_size;
    const sel_host* host = cfg->host;
    unsigned char* buf;
    size_t n = strlen(base), m;
    int ok = 0;

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (n == 0 || n >= URL_BYTES - 64u) {
        return 0;
    }
    sel_cache_init(&cache, NULL, 0);
    if (snprintf(dir, sizeof dir, "%s/sync/sndh", cache.dir) >= (int)sizeof dir || !sel_config_mkdir(dir) ||
        !sel_browse_join(page, sizeof page, dir, "download.php") || !sel_browse_join(tail, sizeof tail, dir, "sndh.tail") ||
        !sel_browse_join(cd, sizeof cd, dir, SEL_SNDH_CD)) {
        return 0;
    }
    buf = host->mem_alloc(host, SEL_SNDH_TAIL_BYTES, SEL_MEM_FAST); /* the page, then the zip's tail */
    if (buf == NULL) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s/download.php", (int)n, base);
    if (sel_sync_net_get_file(url, page, &net->http_requests, &got) != 200) {
        goto done;
    }
    m = slurp(page, buf, SEL_SNDH_TAIL_BYTES);
    year = sel_sndh_newest_year((const char*)buf, m);
    if (year == 0) {
        goto done;
    }
    snprintf(zip, sizeof zip, "%.*s/files/sndh%u_lf.zip", (int)n, base, year);
    if (!sel_sync_net_head_length(zip, &net->http_requests, &size) || size < 22ul) {
        goto done;
    }
    from = size > SEL_SNDH_TAIL_BYTES ? size - SEL_SNDH_TAIL_BYTES : 0ul;
    if (sel_sync_net_get_range(zip, tail, from, size - 1ul, &net->http_requests, &got) != 206) {
        goto done;
    }
    m = slurp(tail, buf, SEL_SNDH_TAIL_BYTES);
    if (!sel_sndh_find_directory(buf, m, &cd_offset, &cd_size) || cd_size == 0 || cd_size > SEL_SNDH_CD_MAX ||
        (unsigned long)cd_offset + cd_size > size) {
        goto done;
    }
    if (sel_sync_net_get_range(zip, cd, cd_offset, (unsigned long)cd_offset + cd_size - 1ul, &net->http_requests,
                               &got) != 206) {
        goto done;
    }
    net->list_bytes = (uint32_t)got;
    ok = 1;
done:
    host->mem_free(host, buf);
    return ok && sel_sndh_sync(cfg, dir, stats);
}
