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

static arexx_theme_fn g_theme_fn;
static void* g_theme_ctx;

void arexx_set_theme_handler(arexx_theme_fn fn, void* ctx) {
    g_theme_fn = fn;
    g_theme_ctx = ctx;
}

/* THEME [path]: the first token without regard to case. Returns 1 and the argument (trimmed, NULL when none) when it is. */
static int is_theme(const char* s, char* path, size_t cap) {
    static const char word[] = "THEME";
    size_t i, n;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    for (i = 0; i < 5u; i++) {
        char ch = s[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        if (ch != word[i]) {
            return 0;
        }
    }
    if (s[5] != '\0' && s[5] != ' ' && s[5] != '\t') {
        return 0;
    }
    s += 5;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) {
        n--;
    }
    if (n >= 2u && s[0] == '"' && s[n - 1] == '"') { /* ARexx scripts quote a path with spaces */
        s++;
        n -= 2u;
    }
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(path, s, n);
    path[n] = '\0';
    return 1;
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
    {   /* I676b: THEME belongs to the UI, not the engine; the registered handler answers it (0: accepted) */
        char path[256];
        if (is_theme(arg0, path, sizeof path)) {
            rc = g_theme_fn == NULL ? 1 : g_theme_fn(g_theme_ctx, path[0] != '\0' ? path : NULL);
            if (rc != 0) {
                fail(out, counters, reply, cap, g_theme_fn == NULL ? "theme_not_supported" : "theme_refused");
                return;
            }
            out->result1 = AREXX_RC_OK;
            out->has_string = 0;
            out->text = reply;
            counters->cmds++;
            return;
        }
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

int arexx_end_batch(int batch_open, arexx_exec_fn exec, void* exec_ctx, char* reply, size_t cap,
                    arexx_counters* counters) {
    arexx_result r;
    if (!batch_open) {
        return 0;
    }
    arexx_dispatch("END", 0, exec, exec_ctx, reply, cap, &r, counters);
    return 1;
}

int arexx_hook_cmdline(const char* script, char* out, size_t cap) {
    if (script == NULL || script[0] == '\0' || strchr(script, '"') != NULL) {
        return 0;
    }
    return snprintf(out, cap, "rx \"%s\"", script) < (int)cap;
}

int arexx_hook_event(int song_opened, const char* script, arexx_launch_fn launch, void* launch_ctx,
                     arexx_hook_stats* st, char* logline, size_t cap) {
    char cmd[512];
    if (!song_opened || script == NULL || script[0] == '\0') {
        return -1;
    }
    if (!arexx_hook_cmdline(script, cmd, sizeof cmd)) {
        snprintf(logline, cap, "hook_command bad_script %s", script);
        st->failed++;
        return 0;
    }
    if (!launch(launch_ctx, script, cmd)) {
        snprintf(logline, cap, "hook_command failed %s", script);
        st->failed++;
        return 0;
    }
    st->fired++;
    return 1;
}

const char* arexx_message_line(unsigned long action, const char* arg0) {
    return (action & 0xFF000000ul) == 0x01000000ul ? arg0 : NULL; /* RXCODEMASK, RXCOMM of rexx/storage.h */
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
