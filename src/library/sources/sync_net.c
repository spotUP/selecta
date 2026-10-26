/* See sync_net.h. */
#include "sync_net.h"

#include "../net/http.h"

#include <stdio.h>
#if defined(__unix__) || defined(__APPLE__)
#include <time.h>
#endif
#include <string.h>

typedef struct net_ctx {
    FILE* f;
    unsigned long bytes;
} net_ctx;

static int file_sink(void* ctx, const void* data, size_t len) {
    net_ctx* c = ctx;

    c->bytes += (unsigned long)len;
    return fwrite(data, 1, len, c->f) == len;
}

/* GETs `url` into `path`, optionally one Range. `want` is the status that counts as success (200 for a whole file,
 * 206 for a range: a server that answers a range with 200 would send the whole file, which is refused). */
static int get_file(const char* url, const char* path, int has_range, unsigned long from, long to, int want,
                    uint32_t* requests, unsigned long* bytes) {
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
    req.has_range = has_range;
    req.range_from = from;
    req.range_to = to;
    err = sel_http_request(&req, file_sink, &c, &res);
    *requests += res.requests;
    fclose(c.f);
    *bytes = c.bytes;
    if (err != SEL_HTTP_OK || res.status != want) {
        remove(path);
    }
    return err == SEL_HTTP_OK ? res.status : 0;
}

/* GETs `url` into `path`. Returns the HTTP status (0 on a transport failure). Any result but 200 removes the file. */
int sel_sync_net_get_file(const char* url, const char* path, uint32_t* requests, unsigned long* bytes) {
    return get_file(url, path, 0, 0, -1, 200, requests, bytes);
}

int sel_sync_net_get_range(const char* url, const char* path, unsigned long from, unsigned long to,
                           uint32_t* requests, unsigned long* bytes) {
    return get_file(url, path, 1, from, (long)to, 206, requests, bytes);
}

static int null_sink(void* ctx, const void* data, size_t len) {
    (void)ctx;
    (void)data;
    (void)len;
    return 1;
}

int sel_sync_net_head_length(const char* url, uint32_t* requests, unsigned long* length) {
    sel_http_req req;
    sel_http_res res;
    sel_http_err err;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.method = "HEAD";
    req.url = url;
    err = sel_http_request(&req, null_sink, NULL, &res);
    *requests += res.requests;
    if (err != SEL_HTTP_OK || res.status != 200 || res.content_length < 0) {
        return 0;
    }
    *length = (unsigned long)res.content_length;
    return 1;
}

void sel_sync_net_pause(const sel_host* host, uint32_t ms) {
#if defined(__unix__) || defined(__APPLE__)
    struct timespec ts;

    (void)host;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
#else
    uint32_t start = host->clock_ms(host);

    while (host->clock_ms(host) - start < ms) {
    }
#endif
}
