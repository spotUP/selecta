/* The built-in HTTP file server for Chromecast playback (I892e2). The receiver fetches the song itself, so Selecta
 * serves exactly one registered file at /<random token> on the LAN interface. No part of a request maps to the
 * filesystem: the path is compared with the token, anything else answers 404. GET and HEAD, Content-Length, one
 * "Range: bytes=" range (206 / 416). Sockets: the one socket layer (src/library/net/net.h, sel_net_listen/accept), so
 * the Amiga build uses the bsdsocket backend. Single threaded: the caller pumps sel_cast_http_poll. */
#ifndef SELECTA_SHARED_CAST_HTTP_H
#define SELECTA_SHARED_CAST_HTTP_H

#include <stddef.h>

#define SEL_CAST_HTTP_PATH_MAX 512
#define SEL_CAST_HTTP_TOKEN_LEN 32
#define SEL_CAST_HTTP_E_FILE -100
#define SEL_CAST_HTTP_E_ENTROPY -101 /* entropy pool not ready: no unguessable token, so no server */

typedef struct sel_cast_http {
    int lh; /* listen handle, -1 when closed */
    unsigned port;
    char token[SEL_CAST_HTTP_TOKEN_LEN + 1];
    char file[SEL_CAST_HTTP_PATH_MAX];
    char mime[48];
    unsigned requests;        /* requests answered (any status) */
    unsigned long bytes_sent; /* body bytes written */
} sel_cast_http;

/* Starts listening on an ephemeral port and registers `file` with its MIME type. Returns 0, a negative SEL_NET_E_*
 * code, SEL_CAST_HTTP_E_FILE when the file cannot be opened, or SEL_CAST_HTTP_E_ENTROPY when the entropy pool is not ready. */
int sel_cast_http_open(sel_cast_http* s, const char* file, const char* mime);

/* "http://<ip>:<port>/<token>" for the receiver; ip is the local address on the link to the receiver
 * (sel_net_local_ip of the control connection). 0 on success. */
int sel_cast_http_url(const sel_cast_http* s, const char* ip, char* out, size_t cap);

/* Accepts one connection within timeout_ms and answers its requests (keep-alive until the peer closes or 5 s of
 * silence). Returns the number of requests answered, 0 when nobody connected, negative on a socket error. */
int sel_cast_http_poll(sel_cast_http* s, unsigned timeout_ms);

void sel_cast_http_close(sel_cast_http* s);

/* Pure: parses the value after "bytes=" against a file of `size` bytes. 1 = satisfiable (first..last inclusive),
 * 0 = not satisfiable, -1 = malformed (serve the whole file). Only the first of several ranges is honoured. */
int sel_cast_http_range(const char* spec, unsigned long size, unsigned long* first, unsigned long* last);

#endif
