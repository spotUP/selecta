/* HTTP/1.1 client (plan 2026-10-25-m3-library.md, I153, D-M3-4). One client for both
 * tiers over the socket table in net.h. GET/PUT/POST/DELETE (and HEAD), chunked
 * transfer decoding, a single Range, up to 5 redirects, no allocation: the body is
 * streamed through a sink. https:// needs TLS (row I154) and fails with
 * SEL_HTTP_E_TLS_UNAVAILABLE until then.
 *
 * Time limits (the bbmp limits): connect 3 s, first response byte 3 s, whole
 * request including redirects 30 s. */
#ifndef SELECTA_LIBRARY_NET_HTTP_H
#define SELECTA_LIBRARY_NET_HTTP_H

#include <stddef.h>

#define SEL_HTTP_CONNECT_MS 3000u
#define SEL_HTTP_FIRST_BYTE_MS 3000u
#define SEL_HTTP_TOTAL_MS 30000u
#define SEL_HTTP_MAX_REDIRECTS 5

typedef enum sel_http_err {
    SEL_HTTP_OK = 0,
    SEL_HTTP_E_URL,
    SEL_HTTP_E_BLOCKED, /* net guard refused the target */
    SEL_HTTP_E_CONNECT,
    SEL_HTTP_E_CONNECT_TIMEOUT,
    SEL_HTTP_E_FIRST_BYTE_TIMEOUT,
    SEL_HTTP_E_TOTAL_TIMEOUT,
    SEL_HTTP_E_IO,
    SEL_HTTP_E_PROTOCOL,
    SEL_HTTP_E_TLS_UNAVAILABLE,
    SEL_HTTP_E_TOO_MANY_REDIRECTS,
    SEL_HTTP_E_SINK /* the sink returned 0 */
} sel_http_err;

typedef struct sel_http_req {
    const char* method;               /* "GET" when NULL */
    const char* url;                  /* http://host[:port]/path */
    const char* const* headers;       /* NULL-terminated "Name: value" lines, or NULL */
    const void* body;                 /* request body for PUT/POST, or NULL */
    size_t body_len;
    int has_range;                    /* send "Range: bytes=from-to" */
    unsigned long range_from;
    long range_to;                    /* inclusive; -1 means open ended */
    unsigned connect_ms;              /* 0 selects the default limits */
    unsigned first_byte_ms;
    unsigned total_ms;
} sel_http_req;

typedef struct sel_http_res {
    int status;                       /* final status code, 0 when none arrived */
    unsigned requests;                /* requests sent, redirects included */
    unsigned redirects;
    unsigned long body_bytes;         /* bytes handed to the sink */
    long content_length;              /* -1 when absent or chunked */
    unsigned long elapsed_ms;
} sel_http_res;

/* Body bytes for the final response; return 0 to abort. */
typedef int (*sel_http_sink)(void* ctx, const void* data, size_t len);

sel_http_err sel_http_request(const sel_http_req* req, sel_http_sink sink, void* ctx, sel_http_res* res);
const char* sel_http_err_name(sel_http_err e);

#endif
