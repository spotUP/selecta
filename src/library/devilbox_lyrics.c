/* DEViLBOX lyrics endpoint client (I193). See devilbox_lyrics.h. */
#include "devilbox_lyrics.h"

#include "net/http.h"
#include "net/mirrors.h"

#include <stdio.h>
#include <string.h>

typedef struct sink {
    char* out;
    size_t cap, len;
} sink;

static int lyrics_sink(void* ctx, const void* data, size_t len) {
    sink* s = ctx;
    size_t room = s->cap - 1u - s->len;

    if (len > room) len = room; /* a longer answer is cut, the transfer goes on */
    memcpy(s->out + s->len, data, len);
    s->len += len;
    s->out[s->len] = '\0';
    return 1;
}

sel_dvb_lyrics_status sel_dvb_lyrics_get_ex(const char* hash, char* out, size_t cap, size_t* len, int* http_status) {
    char url[256];
    sink s;
    sel_http_req req;
    sel_http_res res;

    if (len != NULL) *len = 0;
    if (http_status != NULL) *http_status = 0;
    if (cap < 2u) return SEL_DVB_LYRICS_MISSING;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    s.out = out;
    s.cap = cap;
    s.len = 0;
    out[0] = '\0';
    snprintf(url, sizeof url, "%s/api/lyrics/%s", sel_devilbox_base(), hash);
    req.url = url;
    if (sel_http_request(&req, lyrics_sink, &s, &res) != SEL_HTTP_OK) return SEL_DVB_LYRICS_MISSING;
    if (http_status != NULL) *http_status = res.status;
    if (res.status == 404 && strstr(out, "unknown hash") != NULL) {
        out[0] = '\0';
        return SEL_DVB_LYRICS_UNKNOWN;
    }
    if (res.status != 200 || s.len == 0) {
        out[0] = '\0';
        return SEL_DVB_LYRICS_MISSING;
    }
    if (len != NULL) *len = s.len;
    return SEL_DVB_LYRICS_HIT;
}

sel_dvb_lyrics_status sel_dvb_lyrics_get(const char* hash, char* out, size_t cap, size_t* len) {
    return sel_dvb_lyrics_get_ex(hash, out, cap, len, NULL);
}

static int discard_sink(void* ctx, const void* data, size_t len) {
    (void)ctx;
    (void)data;
    (void)len;
    return 1;
}

/* Same request shape as sel_dvb_loudness_put: Bearer header, method field. The server rejects any other Content-Type
 * than text/plain with 400. */
int sel_dvb_lyrics_put(const char* hash, const char* token, const char* text, size_t len) {
    char url[256], auth[1100];
    const char* headers[3];
    sel_http_req req;
    sel_http_res res;

    if (len == 0u || len > SEL_DVB_LYRICS_UPLOAD_MAX) return 0;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    snprintf(url, sizeof url, "%s/api/lyrics/%s", sel_devilbox_base(), hash);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    headers[0] = auth;
    headers[1] = "Content-Type: text/plain; charset=utf-8";
    headers[2] = NULL;
    req.method = "POST";
    req.url = url;
    req.headers = headers;
    req.body = text;
    req.body_len = len;
    if (sel_http_request(&req, discard_sink, NULL, &res) != SEL_HTTP_OK) return 0;
    return res.status;
}
