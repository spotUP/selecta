/* See sndh_net.h. */
#include "sndh_net.h"

#include <selecta/catalogue.h>
#include <selecta/net.h>
#include <stdio.h>
#include <string.h>

#define URL_BYTES 1100u
#define PATH_BYTES 1100u

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

int sel_sndh_net_sync(const sel_host* host, const char* base, sel_sndh_sync_stats* stats,
                      sel_sndh_net_stats* net) {
    const sel_net* net_svc = host->net != NULL ? host->net(host) : NULL;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char dir[PATH_BYTES - 64u];
    char page[PATH_BYTES];
    char tail[PATH_BYTES];
    char cd[PATH_BYTES];
    char url[URL_BYTES];
    char zip[URL_BYTES];
    unsigned long got = 0, size = 0, from;
    unsigned year;
    uint32_t cd_offset, cd_size;
    unsigned char* buf;
    size_t n = strlen(base), m;
    int ok = 0;

    memset(net, 0, sizeof *net);
    memset(stats, 0, sizeof *stats);
    while (n > 0 && base[n - 1] == '/') {
        n--;
    }
    if (net_svc == NULL || cat == NULL || n == 0 || n >= URL_BYTES - 64u || !cat->cache_dir(host, "sndh", dir, sizeof dir) ||
        snprintf(page, sizeof page, "%s/download.php", dir) >= (int)sizeof page ||
        snprintf(tail, sizeof tail, "%s/sndh.tail", dir) >= (int)sizeof tail ||
        snprintf(cd, sizeof cd, "%s/%s", dir, SEL_SNDH_CD) >= (int)sizeof cd) {
        return 0;
    }
    buf = host->mem_alloc(host, SEL_SNDH_TAIL_BYTES, SEL_MEM_FAST); /* the page, then the zip's tail */
    if (buf == NULL) {
        return 0;
    }
    snprintf(url, sizeof url, "%.*s/download.php", (int)n, base);
    if (net_svc->get_file(host, url, page, &net->http_requests, &got) != 200) {
        goto done;
    }
    m = slurp(page, buf, SEL_SNDH_TAIL_BYTES);
    year = sel_sndh_newest_year((const char*)buf, m);
    if (year == 0) {
        goto done;
    }
    snprintf(zip, sizeof zip, "%.*s/files/sndh%u_lf.zip", (int)n, base, year);
    if (!net_svc->head_length(host, zip, &net->http_requests, &size) || size < 22ul) {
        goto done;
    }
    from = size > SEL_SNDH_TAIL_BYTES ? size - SEL_SNDH_TAIL_BYTES : 0ul;
    if (net_svc->get_range(host, zip, tail, from, size - 1ul, &net->http_requests, &got) != 206) {
        goto done;
    }
    m = slurp(tail, buf, SEL_SNDH_TAIL_BYTES);
    if (!sel_sndh_find_directory(buf, m, &cd_offset, &cd_size) || cd_size == 0 || cd_size > SEL_SNDH_CD_MAX ||
        (unsigned long)cd_offset + cd_size > size) {
        goto done;
    }
    if (net_svc->get_range(host, zip, cd, cd_offset, (unsigned long)cd_offset + cd_size - 1ul, &net->http_requests,
                               &got) != 206) {
        goto done;
    }
    net->list_bytes = (uint32_t)got;
    /* The zip's URL stays beside the directory: the cache takes a tune the site has no loose file for from the zip. */
    if (snprintf(tail, sizeof tail, "%s/%s", dir, SEL_SNDH_ZIPURL) < (int)sizeof tail) {
        FILE* f = fopen(tail, "wb");

        if (f != NULL) {
            fputs(zip, f);
            fclose(f);
        }
    }
    ok = 1;
done:
    host->mem_free(host, buf);
    return ok && sel_sndh_sync(host, dir, stats);
}
