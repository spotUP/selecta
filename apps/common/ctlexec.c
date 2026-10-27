/* One protocol line in, rc and reply out (plan 2026-10-25-m4-arexx section 3). Lifted out of the shell's ctl_handle so
 * the control socket and the ARexx port share it. */
#include "ctlexec.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char* reply;
    size_t cap;
    size_t len;
    unsigned results;
    int overflow;
    int errored;
    char error[256];
    sel_event_sink fwd;
    void* fwd_ctx;
} exec_ctx;

static void exec_sink(void* ctx, const sel_message* m) {
    exec_ctx* x = ctx;

    if (m->id == SEL_EVT_ERROR && !x->errored) {
        const char* code = m->args[0].v.s;
        const char* detail = m->argc > 1 ? m->args[1].v.s : NULL;
        x->errored = 1;
        snprintf(x->error, sizeof x->error, "%s%s%s", code, detail != NULL ? " " : "", detail != NULL ? detail : "");
    } else if (m->id == SEL_EVT_RESULT && !x->overflow) {
        const char* line = m->args[0].v.s;
        size_t n = strlen(line);
        size_t need = n + (x->results > 0 ? 1u : 0u);

        if (x->len + need + 1u > x->cap) {
            x->overflow = 1;
        } else {
            if (x->results > 0) {
                x->reply[x->len++] = '\n';
            }
            memcpy(x->reply + x->len, line, n);
            x->len += n;
            x->reply[x->len] = '\0';
            x->results++;
        }
    }
    if (x->fwd != NULL) {
        x->fwd(x->fwd_ctx, m);
    }
}

int sel_ctl_exec_fwd(sel_engine* e, const char* line, char* reply, size_t cap, int* rc, sel_event_sink fwd,
                     void* fwd_ctx) {
    exec_ctx x;
    char pool[1024];
    int sent;

    memset(&x, 0, sizeof x);
    x.reply = reply;
    x.cap = cap;
    x.fwd = fwd;
    x.fwd_ctx = fwd_ctx;
    if (cap > 0) {
        reply[0] = '\0';
    }
    sent = sel_engine_send_line(e, line, pool, sizeof pool);
    sel_engine_pump(e, exec_sink, &x); /* a rejected line queued its ERROR event */
    if (sent != SEL_CODEC_OK && !x.errored) {
        x.errored = 1;
        snprintf(x.error, sizeof x.error, "%s", sel_codec_strerror(sent));
    }
    if (x.errored) {
        snprintf(reply, cap, "%s", x.error);
        *rc = 10;
        return 0;
    }
    if (x.overflow) {
        snprintf(reply, cap, "result_truncated");
        *rc = 10;
        return 0;
    }
    *rc = 0;
    return 1;
}

int sel_ctl_exec(sel_engine* e, const char* line, char* reply, size_t cap, int* rc) {
    return sel_ctl_exec_fwd(e, line, reply, cap, rc, NULL, NULL);
}
