/* See sync_net.h. */
#include "sync_net.h"

#include "../net/http.h"

#include <stdio.h>
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

/* GETs `url` into `path`. Returns the HTTP status (0 on a transport failure). Any result but 200 removes the file. */
int sel_sync_net_get_file(const char* url, const char* path, uint32_t* requests, unsigned long* bytes) {
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
    *requests += res.requests;
    fclose(c.f);
    *bytes = c.bytes;
    if (err != SEL_HTTP_OK || res.status != 200) {
        remove(path);
    }
    return err == SEL_HTTP_OK ? res.status : 0;
}
