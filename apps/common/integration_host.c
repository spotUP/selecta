/* Host side of integration plugins. See integration_host.h. */
#include "integration_host.h"

#include <selecta/integration.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "net/http.h"
#include "md5.h"

#include <stdlib.h>
#include <string.h>

struct sel_int_session {
    sel_engine* engine;
    sel_int_host ih;
    sel_int_sink sink;
    void* sink_ctx;
    sel_int_on_event_fn on_event;
    sel_int_poll_fn poll;
    sel_int_close_fn close;
    void* state;
};

static int session_send(void* ctx, const char* line) {
    sel_int_session* s = ctx;
    char pool[1024];
    if (sel_engine_send_line(s->engine, line, pool, sizeof pool) != SEL_CODEC_OK) {
        return 1;
    }
    sel_engine_pump(s->engine, s->sink, s->sink_ctx); /* the pool is gone after this call */
    return 0;
}

typedef struct reply_buf {
    char* text;
    size_t cap;
    size_t len;
} reply_buf;

static int reply_sink(void* ctx, const void* data, size_t len) {
    reply_buf* b = ctx;
    size_t room = b->cap - 1u - b->len;
    memcpy(b->text + b->len, data, len < room ? len : room);
    b->len += len < room ? len : room;
    b->text[b->len] = '\0';
    return 1;
}

/* The one HTTP client (src/library/net/http.c); integrations never open sockets themselves. */
static int session_http_post(void* ctx, const char* url, const char* body, char* reply, size_t reply_cap,
                             int* status) {
    static const char* const headers[] = {"Content-Type: application/x-www-form-urlencoded", NULL};
    sel_http_req req;
    sel_http_res res;
    reply_buf b;
    (void)ctx;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    b.text = reply;
    b.cap = reply_cap;
    b.len = 0;
    reply[0] = '\0';
    req.method = "POST";
    req.url = url;
    req.headers = headers;
    req.body = body;
    req.body_len = strlen(body);
    if (sel_http_request(&req, reply_sink, &b, &res) != SEL_HTTP_OK) {
        *status = 0;
        return 1;
    }
    *status = res.status;
    return 0;
}

static void session_md5_hex(void* ctx, const void* data, size_t len, char out[33]) {
    sel_md5 m;
    (void)ctx;
    sel_md5_init(&m);
    sel_md5_update(&m, data, len);
    sel_md5_hex(&m, out);
}

static const sel_tag* plugin_hooks(const char* id) {
    const sel_tag* desc = sel_registry_find(SEL_PLUGIN_KIND_INTEGRATION, id);
    return desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
}

sel_int_session* sel_int_session_open(sel_engine* e, const char* id, sel_int_sink sink, void* sink_ctx) {
    const sel_tag* hooks = plugin_hooks(id);
    sel_int_session* s;
    sel_int_open_fn open;
    if (hooks == NULL) {
        return NULL;
    }
    open = (sel_int_open_fn)sel_tag_find(hooks, SEL_INT_OPEN, 0);
    s = calloc(1, sizeof *s);
    if (open == NULL || s == NULL) {
        free(s);
        return NULL;
    }
    s->engine = e;
    s->sink = sink;
    s->sink_ctx = sink_ctx;
    s->ih.ctx = s;
    s->ih.send_command = session_send;
    s->ih.http_post = session_http_post;
    s->ih.md5_hex = session_md5_hex;
    s->on_event = (sel_int_on_event_fn)sel_tag_find(hooks, SEL_INT_ON_EVENT, 0);
    s->poll = (sel_int_poll_fn)sel_tag_find(hooks, SEL_INT_POLL, 0);
    s->close = (sel_int_close_fn)sel_tag_find(hooks, SEL_INT_CLOSE, 0);
    s->state = open(sel_engine_plugin_host(e), &s->ih);
    if (s->state == NULL) {
        free(s);
        return NULL;
    }
    return s;
}

int sel_int_login(sel_engine* e, const char* id, const char* token, char* out, size_t cap) {
    const sel_tag* hooks = plugin_hooks(id);
    sel_int_login_fn login = hooks != NULL ? (sel_int_login_fn)sel_tag_find(hooks, SEL_INT_LOGIN, 0) : NULL;
    sel_int_host ih;
    if (login == NULL) {
        return -1;
    }
    memset(&ih, 0, sizeof ih); /* no session: send_command stays NULL */
    ih.http_post = session_http_post;
    ih.md5_hex = session_md5_hex;
    return login(sel_engine_plugin_host(e), &ih, token, out, cap);
}

void sel_int_session_event(sel_int_session* s, const sel_message* evt) {
    if (s != NULL && s->on_event != NULL) {
        s->on_event(s->state, evt, sel_engine_state(s->engine));
    }
}

void sel_int_session_poll(sel_int_session* s) {
    if (s != NULL && s->poll != NULL) {
        s->poll(s->state);
    }
}

void sel_int_session_close(sel_int_session* s) {
    if (s != NULL) {
        if (s->close != NULL) {
            s->close(s->state);
        }
        free(s);
    }
}
