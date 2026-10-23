/* Host side of integration plugins. See integration_host.h. */
#include "integration_host.h"

#include <selecta/integration.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include <stdlib.h>

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

sel_int_session* sel_int_session_open(sel_engine* e, const sel_host* host, const char* id,
                                      sel_int_sink sink, void* sink_ctx) {
    const sel_tag* desc = sel_registry_find(SEL_PLUGIN_KIND_INTEGRATION, id);
    const sel_tag* hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
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
    s->on_event = (sel_int_on_event_fn)sel_tag_find(hooks, SEL_INT_ON_EVENT, 0);
    s->poll = (sel_int_poll_fn)sel_tag_find(hooks, SEL_INT_POLL, 0);
    s->close = (sel_int_close_fn)sel_tag_find(hooks, SEL_INT_CLOSE, 0);
    s->state = open(host, &s->ih);
    if (s->state == NULL) {
        free(s);
        return NULL;
    }
    return s;
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
