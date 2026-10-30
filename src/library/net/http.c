/* HTTP/1.1 client; interface in http.h. Streams the body through a sink with
 * fixed buffers only. Every request sends `Connection: close`, so a body without a
 * length ends at the peer's shutdown. */
#include "http.h"

#include "net.h"
#include "tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDR_CAP 8192
#define URL_CAP SEL_HTTP_URL_MAX

typedef struct url_parts {
    char host[256];
    unsigned port;
    char path[URL_CAP];
    int tls;
} url_parts;

static int ci_prefix(const char* s, const char* prefix) {
    for (; *prefix != '\0'; s++, prefix++) {
        char a = *s, b = *prefix;
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a + 32);
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int parse_url(const char* url, url_parts* u) {
    const char* p = url;
    const char* hs;
    size_t n;
    if (ci_prefix(p, "http://")) {
        p += 7;
        u->tls = 0;
        u->port = 80;
    } else if (ci_prefix(p, "https://")) {
        p += 8;
        u->tls = 1;
        u->port = 443;
    } else {
        return 0;
    }
    hs = p;
    while (*p != '\0' && *p != '/' && *p != ':' && *p != '?') {
        p++;
    }
    n = (size_t)(p - hs);
    if (n == 0 || n >= sizeof u->host) {
        return 0;
    }
    memcpy(u->host, hs, n);
    u->host[n] = '\0';
    if (*p == ':') {
        char* end;
        unsigned long v = strtoul(p + 1, &end, 10);
        if (end == p + 1 || v == 0 || v > 65535ul) {
            return 0;
        }
        u->port = (unsigned)v;
        p = end;
    }
    if (*p == '\0') {
        strcpy(u->path, "/");
    } else if (*p == '?') {
        if (strlen(p) + 2 > sizeof u->path) {
            return 0;
        }
        u->path[0] = '/';
        strcpy(u->path + 1, p);
    } else {
        if (strlen(p) >= sizeof u->path) {
            return 0;
        }
        strcpy(u->path, p);
    }
    return 1;
}

/* Resolve a Location value against the current request into an absolute URL. */
static int resolve_location(const url_parts* cur, const char* loc, char* out, size_t cap) {
    int n;
    if (ci_prefix(loc, "http://") || ci_prefix(loc, "https://")) {
        n = snprintf(out, cap, "%s", loc);
    } else {
        char hostport[300];
        const char* scheme = cur->tls ? "https" : "http";
        unsigned def = cur->tls ? 443u : 80u;
        if (cur->port == def) {
            snprintf(hostport, sizeof hostport, "%s", cur->host);
        } else {
            snprintf(hostport, sizeof hostport, "%s:%u", cur->host, cur->port);
        }
        if (loc[0] == '/') {
            n = snprintf(out, cap, "%s://%s%s", scheme, hostport, loc);
        } else {
            char dir[URL_CAP];
            char* slash;
            snprintf(dir, sizeof dir, "%s", cur->path);
            slash = strrchr(dir, '/');
            if (slash != NULL) {
                slash[1] = '\0';
            }
            n = snprintf(out, cap, "%s://%s%s%s", scheme, hostport, dir, loc);
        }
    }
    return n > 0 && (size_t)n < cap;
}

typedef struct clock_state {
    unsigned long start;
    unsigned long deadline;
} clock_state;

/* Remaining ms to the total deadline, 0 when over. */
static unsigned remaining(const clock_state* c) {
    unsigned long now = sel_net_now_ms();
    return now >= c->deadline ? 0u : (unsigned)(c->deadline - now);
}

static sel_http_err map_net(int rc, int first_byte) {
    switch (rc) {
    case SEL_NET_E_BLOCKED: return SEL_HTTP_E_BLOCKED;
    case SEL_NET_E_TIMEOUT: return first_byte ? SEL_HTTP_E_FIRST_BYTE_TIMEOUT : SEL_HTTP_E_TOTAL_TIMEOUT;
    case SEL_NET_E_RESOLVE:
    case SEL_NET_E_CONNECT:
    case SEL_NET_E_TABLE: return SEL_HTTP_E_CONNECT;
    default: return SEL_HTTP_E_IO;
    }
}

/* Receive with the total deadline applied; first_ms (0 = none) additionally caps the wait. */
static int recv_dl(int h, void* buf, size_t cap, const clock_state* c, unsigned first_ms, sel_http_err* err) {
    unsigned rem = remaining(c);
    unsigned wait = rem;
    int is_first = 0, n;
    if (first_ms != 0 && first_ms < rem) {
        wait = first_ms;
        is_first = 1;
    }
    if (rem == 0) {
        *err = SEL_HTTP_E_TOTAL_TIMEOUT;
        return -1;
    }
    n = sel_stream_recv(h, buf, cap, wait);
    if (n == SEL_NET_E_TIMEOUT) {
        *err = is_first ? SEL_HTTP_E_FIRST_BYTE_TIMEOUT : SEL_HTTP_E_TOTAL_TIMEOUT;
        return -1;
    }
    return n;
}

/* Chunked-encoding decoder state. */
typedef enum { CH_SIZE, CH_EXT, CH_DATA, CH_DATA_CR, CH_DATA_LF, CH_DONE } chunk_state;

typedef struct body_ctx {
    sel_http_sink sink;
    void* ctx;
    unsigned long bytes;
    chunk_state cs;
    unsigned long chunk_left;
    unsigned long size_acc;
    int size_digits;
    int chunked;
    int aborted;
} body_ctx;

static int emit(body_ctx* b, const char* data, size_t len) {
    if (len == 0) {
        return 1;
    }
    if (b->sink != NULL && !b->sink(b->ctx, data, len)) {
        b->aborted = 1;
        return 0;
    }
    b->bytes += len;
    return 1;
}

static int chunk_size_done(body_ctx* b) {
    if (b->size_digits == 0) {
        return 0;
    }
    b->chunk_left = b->size_acc;
    b->size_acc = 0;
    b->size_digits = 0;
    b->cs = b->chunk_left == 0 ? CH_DONE : CH_DATA;
    return 1;
}

/* Feed raw bytes. Returns 0 on protocol error or sink abort, else 1. */
static int body_feed(body_ctx* b, const char* d, size_t len) {
    if (!b->chunked) {
        return emit(b, d, len);
    }
    while (len > 0 && b->cs != CH_DONE) {
        switch (b->cs) {
        case CH_SIZE: {
            char c = *d++;
            int digit = -1;
            len--;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
            }
            if (digit >= 0) {
                b->size_acc = b->size_acc * 16ul + (unsigned long)digit;
                b->size_digits++;
            } else if (c == '\n') {
                if (!chunk_size_done(b)) {
                    return 0;
                }
            } else if (c == ';' || c == ' ' || c == '\t' || c == '\r') {
                b->cs = CH_EXT;
            } else {
                return 0;
            }
            break;
        }
        case CH_EXT: {
            char c = *d++;
            len--;
            if (c == '\n' && !chunk_size_done(b)) {
                return 0;
            }
            break;
        }
        case CH_DATA: {
            size_t n = len < b->chunk_left ? len : (size_t)b->chunk_left;
            if (!emit(b, d, n)) {
                return 0;
            }
            d += n;
            len -= n;
            b->chunk_left -= n;
            if (b->chunk_left == 0) {
                b->cs = CH_DATA_CR;
            }
            break;
        }
        case CH_DATA_CR:
            if (*d == '\r') {
                d++;
                len--;
                b->cs = CH_DATA_LF;
            } else if (*d == '\n') {
                d++;
                len--;
                b->cs = CH_SIZE;
            } else {
                return 0;
            }
            break;
        case CH_DATA_LF:
            if (*d != '\n') {
                return 0;
            }
            d++;
            len--;
            b->cs = CH_SIZE;
            break;
        default:
            break;
        }
    }
    return 1;
}

typedef struct resp_head {
    int status;
    long content_length;
    int chunked;
    unsigned icy_metaint;
    char content_type[40];
    char location[URL_CAP];
} resp_head;

static int header_is(const char* line, const char* name) {
    return ci_prefix(line, name);
}

static int parse_head(char* h, resp_head* r) {
    char* line = h;
    char* eol;
    int first = 1;
    memset(r, 0, sizeof *r);
    r->content_length = -1;
    while (line != NULL && *line != '\0') {
        eol = strstr(line, "\r\n");
        if (eol != NULL) {
            *eol = '\0';
        }
        if (first) {
            if (strncmp(line, "ICY ", 4) == 0 && strlen(line) >= 7) { /* Shoutcast v1: "ICY 200 OK" (I196a) */
                r->status = atoi(line + 4);
            } else if (strncmp(line, "HTTP/1.", 7) != 0 || strlen(line) < 12) {
                return 0;
            } else {
                r->status = atoi(line + 9);
            }
            if (r->status < 100) {
                return 0;
            }
            first = 0;
        } else if (header_is(line, "content-length:")) {
            r->content_length = atol(line + 15);
        } else if (header_is(line, "icy-metaint:")) {
            r->icy_metaint = (unsigned)strtoul(line + 12, NULL, 10);
        } else if (header_is(line, "content-type:")) {
            const char* v = line + 13;
            size_t n = 0;
            while (*v == ' ') {
                v++;
            }
            while (v[n] != '\0' && v[n] != ';' && v[n] != ' ' && n + 1 < sizeof r->content_type) {
                r->content_type[n] = (char)((v[n] >= 'A' && v[n] <= 'Z') ? v[n] + 32 : v[n]);
                n++;
            }
            r->content_type[n] = '\0';
        } else if (header_is(line, "transfer-encoding:")) {
            const char* v = line + 18;
            while (*v == ' ') {
                v++;
            }
            r->chunked = ci_prefix(v, "chunked");
        } else if (header_is(line, "location:")) {
            const char* v = line + 9;
            while (*v == ' ') {
                v++;
            }
            if (strlen(v) < sizeof r->location) {
                strcpy(r->location, v);
            }
        }
        line = eol != NULL ? eol + 2 : NULL;
    }
    return !first;
}

static int build_request(char* out, size_t cap, const sel_http_req* req, const char* method, const url_parts* u, int with_body) {
    size_t n = 0;
    int w;
    unsigned def = u->tls ? 443u : 80u;
    const char* const* hp;
    if (u->port == def) {
        w = snprintf(out, cap, "%s %s HTTP/1.1\r\nHost: %s\r\n", method, u->path, u->host);
    } else {
        w = snprintf(out, cap, "%s %s HTTP/1.1\r\nHost: %s:%u\r\n", method, u->path, u->host, u->port);
    }
    if (w < 0 || (size_t)w >= cap) {
        return -1;
    }
    n = (size_t)w;
    w = snprintf(out + n, cap - n, "User-Agent: %s\r\nAccept: */*\r\nConnection: close\r\n",
                 req->user_agent != NULL && req->user_agent[0] != '\0' ? req->user_agent : SEL_HTTP_DEFAULT_UA);
    if (w < 0 || (size_t)w >= cap - n) {
        return -1;
    }
    n += (size_t)w;
    if (req->has_range) {
        if (req->range_to >= 0) {
            w = snprintf(out + n, cap - n, "Range: bytes=%lu-%ld\r\n", req->range_from, req->range_to);
        } else {
            w = snprintf(out + n, cap - n, "Range: bytes=%lu-\r\n", req->range_from);
        }
        if (w < 0 || (size_t)w >= cap - n) {
            return -1;
        }
        n += (size_t)w;
    }
    for (hp = req->headers; hp != NULL && *hp != NULL; hp++) {
        w = snprintf(out + n, cap - n, "%s\r\n", *hp);
        if (w < 0 || (size_t)w >= cap - n) {
            return -1;
        }
        n += (size_t)w;
    }
    if (with_body) {
        w = snprintf(out + n, cap - n, "Content-Length: %lu\r\n", (unsigned long)req->body_len);
        if (w < 0 || (size_t)w >= cap - n) {
            return -1;
        }
        n += (size_t)w;
    }
    if (n + 2 >= cap) {
        return -1;
    }
    out[n++] = '\r';
    out[n++] = '\n';
    return (int)n;
}

const char* sel_http_err_name(sel_http_err e) {
    switch (e) {
    case SEL_HTTP_OK: return "ok";
    case SEL_HTTP_E_URL: return "bad_url";
    case SEL_HTTP_E_BLOCKED: return "net_blocked";
    case SEL_HTTP_E_CONNECT: return "connect_failed";
    case SEL_HTTP_E_CONNECT_TIMEOUT: return "connect_timeout";
    case SEL_HTTP_E_FIRST_BYTE_TIMEOUT: return "first_byte_timeout";
    case SEL_HTTP_E_TOTAL_TIMEOUT: return "total_timeout";
    case SEL_HTTP_E_IO: return "io_error";
    case SEL_HTTP_E_PROTOCOL: return "protocol_error";
    case SEL_HTTP_E_TLS_UNAVAILABLE: return "tls_unavailable";
    case SEL_HTTP_E_TLS_VERIFY: return "tls_verify_failed";
    case SEL_HTTP_E_TLS_HANDSHAKE: return "tls_handshake_failed";
    case SEL_HTTP_E_TOO_MANY_REDIRECTS: return "too_many_redirects";
    case SEL_HTTP_E_SINK: return "sink_aborted";
    }
    return "unknown";
}

/* One request/response on a fresh connection. On a redirect fills *redirect_to
 * (non-empty) and returns OK. */
static sel_http_err exchange(const sel_http_req* req, const char* method, const url_parts* u, int with_body, sel_http_sink sink,
                             void* ctx, sel_http_res* res, const clock_state* clk, char* redirect_to, size_t redirect_cap) {
    char hdr[HDR_CAP];
    char buf[4096];
    unsigned conn_ms = req->connect_ms != 0 ? req->connect_ms : SEL_HTTP_CONNECT_MS;
    unsigned fb_ms = req->first_byte_ms != 0 ? req->first_byte_ms : SEL_HTTP_FIRST_BYTE_MS;
    unsigned rem = remaining(clk);
    sel_http_err err = SEL_HTTP_OK;
    resp_head head;
    body_ctx b;
    size_t have = 0;
    char* end = NULL;
    int h, n, rq;
    int no_body;

    memset(&b, 0, sizeof b);
    b.sink = sink;
    b.ctx = ctx;
    b.cs = CH_SIZE;
    if (rem == 0) {
        return SEL_HTTP_E_TOTAL_TIMEOUT;
    }
    h = sel_net_open(u->host, u->port, conn_ms < rem ? conn_ms : rem);
    if (h < 0) {
        return h == SEL_NET_E_TIMEOUT ? SEL_HTTP_E_CONNECT_TIMEOUT : map_net(h, 0);
    }
    if (u->tls) {
        int t = sel_tls_open(h, u->host, remaining(clk));
        if (t != SEL_TLS_OK) {
            sel_stream_close(h);
            switch (t) {
            case SEL_TLS_E_UNAVAILABLE: return SEL_HTTP_E_TLS_UNAVAILABLE;
            case SEL_TLS_E_VERIFY: return SEL_HTTP_E_TLS_VERIFY;
            case SEL_TLS_E_TIMEOUT: return SEL_HTTP_E_TOTAL_TIMEOUT;
            default: return SEL_HTTP_E_TLS_HANDSHAKE;
            }
        }
    }
    res->requests++;
    rq = build_request(hdr, sizeof hdr, req, method, u, with_body);
    if (rq < 0) {
        sel_stream_close(h);
        return SEL_HTTP_E_URL;
    }
    n = sel_stream_send(h, hdr, (size_t)rq, remaining(clk));
    if (n == 0 && with_body && req->body_len > 0) {
        n = sel_stream_send(h, req->body, req->body_len, remaining(clk));
    }
    if (n != 0) {
        sel_stream_close(h);
        return n == SEL_NET_E_TIMEOUT ? SEL_HTTP_E_TOTAL_TIMEOUT : SEL_HTTP_E_IO;
    }
    /* Read until the blank line that ends the head. */
    for (;;) {
        unsigned first = have == 0 ? fb_ms : 0u;
        if (have + 1 >= sizeof hdr) {
            err = SEL_HTTP_E_PROTOCOL;
            goto done;
        }
        n = recv_dl(h, hdr + have, sizeof hdr - 1 - have, clk, first, &err);
        if (n < 0) {
            if (err == SEL_HTTP_OK) {
                err = n == SEL_NET_E_CLOSED ? SEL_HTTP_E_PROTOCOL : SEL_HTTP_E_IO;
            }
            goto done;
        }
        have += (size_t)n;
        hdr[have] = '\0';
        end = strstr(hdr, "\r\n\r\n");
        if (end != NULL) {
            break;
        }
    }
    *end = '\0';
    if (!parse_head(hdr, &head)) {
        err = SEL_HTTP_E_PROTOCOL;
        goto done;
    }
    res->status = head.status;
    res->content_length = head.chunked ? -1 : head.content_length;
    res->icy_metaint = head.icy_metaint;
    memcpy(res->content_type, head.content_type, sizeof res->content_type);
    if (head.status >= 300 && head.status < 400 && head.status != 304 && head.location[0] != '\0') {
        if (!resolve_location(u, head.location, redirect_to, redirect_cap)) {
            err = SEL_HTTP_E_PROTOCOL;
        }
        goto done;
    }
    no_body = strcmp(method, "HEAD") == 0 || head.status == 204 || head.status == 304 || (head.status >= 100 && head.status < 200);
    b.chunked = head.chunked;
    if (!no_body) {
        char* body = end + 4;
        size_t got = have - (size_t)(body - hdr);
        unsigned long want = head.chunked || head.content_length < 0 ? 0ul : (unsigned long)head.content_length;
        int bounded = !head.chunked && head.content_length >= 0;
        if (bounded && got > want) {
            got = (size_t)want;
        }
        if (!body_feed(&b, body, got)) {
            err = b.aborted ? SEL_HTTP_E_SINK : SEL_HTTP_E_PROTOCOL;
            goto done;
        }
        if (bounded && b.bytes >= want) {
            goto done;
        }
        while (b.cs != CH_DONE || !head.chunked) {
            n = recv_dl(h, buf, sizeof buf, clk, 0, &err);
            if (n < 0) {
                if (err != SEL_HTTP_OK) {
                    goto done;
                }
                if (n == SEL_NET_E_CLOSED && !head.chunked && !bounded) {
                    break;
                }
                err = SEL_HTTP_E_IO;
                goto done;
            }
            if (bounded && b.bytes + (unsigned long)n > want) {
                n = (int)(want - b.bytes);
            }
            if (!body_feed(&b, buf, (size_t)n)) {
                err = b.aborted ? SEL_HTTP_E_SINK : SEL_HTTP_E_PROTOCOL;
                goto done;
            }
            if (bounded && b.bytes >= want) {
                break;
            }
        }
    }
done:
    sel_stream_close(h);
    res->body_bytes = b.bytes;
    return err;
}

sel_http_err sel_http_request(const sel_http_req* req, sel_http_sink sink, void* ctx, sel_http_res* res) {
    clock_state clk;
    char cur[URL_CAP];
    char next[URL_CAP];
    const char* method = req->method != NULL ? req->method : "GET";
    int with_body = strcmp(method, "PUT") == 0 || strcmp(method, "POST") == 0 || strcmp(method, "DELETE") == 0;
    unsigned total = req->total_ms != 0 ? req->total_ms : SEL_HTTP_TOTAL_MS;
    sel_http_err err;

    memset(res, 0, sizeof *res);
    res->content_length = -1;
    clk.start = sel_net_now_ms();
    clk.deadline = clk.start + total;
    if (req->url == NULL || strlen(req->url) >= sizeof cur) {
        return SEL_HTTP_E_URL;
    }
    strcpy(cur, req->url);
    for (;;) {
        url_parts u;
        if (!parse_url(cur, &u)) {
            err = SEL_HTTP_E_URL;
            break;
        }
        next[0] = '\0';
        err = exchange(req, method, &u, with_body, sink, ctx, res, &clk, next, sizeof next);
        if (err != SEL_HTTP_OK || next[0] == '\0') {
            break;
        }
        if (res->redirects >= SEL_HTTP_MAX_REDIRECTS) {
            err = SEL_HTTP_E_TOO_MANY_REDIRECTS;
            break;
        }
        res->redirects++;
        /* 301/302/303 turn a write into a body-less GET (RFC 9110); 307/308 keep it. */
        if (with_body && res->status != 307 && res->status != 308) {
            method = "GET";
            with_body = 0;
        }
        strcpy(cur, next);
    }
    res->elapsed_ms = sel_net_now_ms() - clk.start;
    return err;
}
