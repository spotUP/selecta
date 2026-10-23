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

sel_dvb_lyrics_status sel_dvb_lyrics_get(const char* hash, char* out, size_t cap, size_t* len) {
    char url[256];
    sink s;
    sel_http_req req;
    sel_http_res res;

    if (len != NULL) *len = 0;
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
