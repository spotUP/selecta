/* URL and pipe song sources for OPEN. See stream_open.h. */
#include "stream_open.h"

#include "net/http.h"

#include <selecta/tags.h>

#include <stdio.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
#define SEL_STREAM_MAX (24u * 1024u * 1024u)
#else
#define SEL_STREAM_MAX (256u * 1024u * 1024u)
#endif

static uint32_t g_bytes, g_reads;

typedef struct sbuf {
    const sel_host* host;
    uint8_t* p;
    size_t len, cap;
} sbuf;

static int sbuf_add(sbuf* b, const void* data, size_t n) {
    if (b->len + n > SEL_STREAM_MAX) {
        return 0;
    }
    if (b->len + n + 1u > b->cap) {
        size_t cap = b->cap == 0 ? 65536u : b->cap;
        uint8_t* np;
        while (cap < b->len + n + 1u) {
            cap *= 2u;
        }
        np = b->host->mem_alloc(b->host, cap, SEL_MEM_FAST);
        if (np == NULL) {
            return 0;
        }
        if (b->len > 0) {
            memcpy(np, b->p, b->len);
        }
        if (b->p != NULL) {
            b->host->mem_free(b->host, b->p);
        }
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->len, data, n);
    b->len += n;
    return 1;
}

static int sink(void* ctx, const void* data, size_t len) {
    return sbuf_add((sbuf*)ctx, data, len);
}

int sel_stream_is_url(const char* path) {
    return path != NULL && (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0);
}

#if defined(__amigaos__) || defined(AMIGA)
int sel_stream_is_pipe(const char* path) {
    (void)path;
    return 0;
}
static int read_pipe(sbuf* b, const char* cmd) {
    (void)b;
    (void)cmd;
    return 0;
}
#else
int sel_stream_is_pipe(const char* path) {
    return path != NULL && strncmp(path, "pipe:", 5) == 0 && path[5] != '\0';
}

static int read_pipe(sbuf* b, const char* cmd) {
    FILE* f = popen(cmd, "r");
    unsigned char chunk[8192];
    size_t n;
    int ok = 1;
    if (f == NULL) {
        return 0;
    }
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) {
        if (!sbuf_add(b, chunk, n)) {
            ok = 0;
            break;
        }
    }
    return pclose(f) == 0 && ok;
}
#endif

uint8_t* sel_stream_read(const sel_host* host, const char* path, size_t* size) {
    sbuf b;
    int ok;
    memset(&b, 0, sizeof b);
    b.host = host;
    if (sel_stream_is_url(path)) {
        sel_http_req req;
        sel_http_res res;
        memset(&req, 0, sizeof req);
        memset(&res, 0, sizeof res);
        req.url = path;
        ok = sel_http_request(&req, sink, &b, &res) == SEL_HTTP_OK && res.status >= 200 && res.status < 300;
    } else if (sel_stream_is_pipe(path)) {
        ok = read_pipe(&b, path + 5);
    } else {
        return NULL;
    }
    if (!ok || b.len == 0) {
        if (b.p != NULL) {
            host->mem_free(host, b.p);
        }
        return NULL;
    }
    b.p[b.len] = 0;
    *size = b.len;
    g_bytes = (uint32_t)b.len;
    g_reads++;
    return b.p;
}

uint32_t sel_stream_bytes(void) {
    return g_bytes;
}

uint32_t sel_stream_reads(void) {
    return g_reads;
}
