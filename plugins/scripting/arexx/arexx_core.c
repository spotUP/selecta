#include "arexx.h"
#include <stdio.h>
#include <string.h>

static void fail(arexx_result* out, arexx_counters* c, char* reply, size_t cap, const char* text) {
    snprintf(reply, cap, "%s", text);
    out->result1 = AREXX_RC_FAILED;
    out->has_string = 0;
    out->text = reply;
    c->errors++;
}

/* The first token, compared with IDLE without regard to case. */
static int is_idle(const char* s) {
    static const char word[] = "IDLE";
    size_t i;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    for (i = 0; i < 4u; i++) {
        char ch = s[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        if (ch != word[i]) {
            return 0;
        }
    }
    return s[4] == '\0' || s[4] == ' ' || s[4] == '\t';
}

void arexx_dispatch(const char* arg0, int want_result, arexx_exec_fn exec, void* exec_ctx, char* reply, size_t cap,
                    arexx_result* out, arexx_counters* counters) {
    int rc = 0;
    reply[0] = '\0';
    if (arg0 == NULL || arg0[0] == '\0') {
        fail(out, counters, reply, cap, "empty_line");
        return;
    }
    if (strlen(arg0) >= AREXX_LINE_MAX) {
        fail(out, counters, reply, cap, "overflow");
        return;
    }
    if (is_idle(arg0)) {
        fail(out, counters, reply, cap, "idle_not_supported_over_arexx");
        return;
    }
    exec(exec_ctx, arg0, reply, cap, &rc);
    if (rc != 0) {
        out->result1 = AREXX_RC_FAILED; /* the only failure code besides closing */
        out->has_string = 0;
        out->text = reply;
        counters->errors++;
        return;
    }
    out->result1 = AREXX_RC_OK;
    out->has_string = want_result;
    out->text = reply;
    counters->cmds++;
}

void arexx_closing_result(arexx_result* out, arexx_counters* counters) {
    out->result1 = AREXX_RC_CLOSING;
    out->has_string = 0;
    out->text = "port_closing";
    counters->errors++;
}

int arexx_port_name(unsigned n, char* out, size_t cap) {
    if (n > 98u) {
        return 0;
    }
    if (n == 0u) {
        snprintf(out, cap, "SELECTA");
    } else {
        snprintf(out, cap, "SELECTA.%u", n + 1u);
    }
    return 1;
}

void arexx_stats_line(char* out, size_t cap, const char* port, const arexx_counters* c) {
    if (port == NULL) {
        snprintf(out, cap, "arexx=off");
    } else {
        snprintf(out, cap, "arexx_port=%s arexx_cmds=%u arexx_errors=%u", port, c->cmds, c->errors);
    }
}
