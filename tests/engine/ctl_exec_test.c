/* M4-AX-0-1 ctl-exec-result and M4-AX-0-2 commands-reflection-format (plan 2026-10-25-m4-arexx): the shared reply
 * layer sel_ctl_exec returns rc and the RESULT lines of one protocol line.
 *   ctl_exec_test status    STATUS on a fresh engine: rc 0, state=stopped
 *   ctl_exec_test commands  every COMMANDS line is "VERB [arg ...]<TAB>Label<TAB>args=.. area=..", one per verb
 *   ctl_exec_test limits    M4-AX-1-4: COMMANDS in half the reply cap, lines <= 255, long request, truncation, case
 *   ctl_exec_test lasterror STATUS carries last_error= (plan section 8a Q6)
 *   ctl_exec_test quitaction  the action table has File/Quit sending QUIT and STATUS stays exempt (M4-AX-2-5)
 *   ctl_exec_test quit      QUIT sets the shell flag; inside BEGIN it waits for END (M4-AX-2-4) */
#include "ctlexec.h"
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>
#include <selecta/uimodel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c)                                                                                  \
    do {                                                                                          \
        if (!(c)) {                                                                               \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                           \
            fails++;                                                                              \
        }                                                                                         \
    } while (0)

static int is_upper(char c) { return c >= 'A' && c <= 'Z'; }

/* ^[A-Z_]+( [a-z_\[\].]+)*\t[A-Z][^\t]+\targs= */
static int line_ok(const char* s) {
    const char* p = s;
    if (!is_upper(*p)) {
        return 0;
    }
    while (is_upper(*p) || *p == '_') {
        p++;
    }
    while (*p == ' ') {
        p++;
        if (!((*p >= 'a' && *p <= 'z') || *p == '_' || *p == '[')) {
            return 0;
        }
        while ((*p >= 'a' && *p <= 'z') || *p == '_' || *p == '[' || *p == ']' || *p == '.') {
            p++;
        }
    }
    if (*p++ != '\t' || !is_upper(*p)) {
        return 0;
    }
    p++;
    if (*p == '\t' || *p == '\0') {
        return 0;
    }
    while (*p != '\t' && *p != '\0') {
        p++;
    }
    return strncmp(p, "\targs=", 6) == 0;
}

/* I320: COMMANDS delivers one RESULT event per verb, each line intact at sink time (the old LOG ring of 8 slots
 * overwrote lines before the sink read them). */
typedef struct {
    unsigned results;
    unsigned intact;
    unsigned logs;
} raw_count;

static void raw_sink(void* ctx, const sel_message* m) {
    raw_count* c = ctx;
    if (m->id == SEL_EVT_RESULT) {
        const char* s = m->args[0].v.s;
        const char* name = sel_cmd_name(c->results < SEL_CMD_COUNT ? c->results : 0);
        size_t n = strlen(name);
        c->intact += strncmp(s, name, n) == 0 && (s[n] == ' ' || s[n] == '\t');
        c->results++;
    } else if (m->id == SEL_EVT_LOG) {
        c->logs++;
    }
}

int main(int argc, char** argv) {
    sel_engine_config cfg;
    sel_engine* e;
    static char reply[SEL_CTL_REPLY_MAX];
    int rc = -1;
    const char* mode = argc > 1 ? argv[1] : "status";

    memset(&cfg, 0, sizeof cfg);
    e = sel_engine_create(sel_host_default(), &cfg);
    CHECK(e != NULL);
    if (e == NULL) {
        return 1;
    }
    if (strcmp(mode, "status") == 0) {
        CHECK(sel_ctl_exec(e, "STATUS", reply, sizeof reply, &rc) == 1);
        CHECK(rc == 0);
        CHECK(strstr(reply, "state=stopped") != NULL);
    } else if (strcmp(mode, "limits") == 0) {
        static char big[1200];
        char small[100];
        size_t longest = 0, cur = 0, i;
        CHECK(SEL_CTL_REPLY_MAX == 16384u);
        CHECK(sel_ctl_exec(e, "COMMANDS", reply, sizeof reply, &rc) == 1);
        CHECK(strlen(reply) * 2 < SEL_CTL_REPLY_MAX);
        for (i = 0;; i++) {
            if (reply[i] == '\n' || reply[i] == '\0') {
                longest = cur > longest ? cur : longest;
                cur = 0;
                if (reply[i] == '\0') {
                    break;
                }
            } else {
                cur++;
            }
        }
        CHECK(longest <= 255u);
        printf("commands_bytes=%u longest_line=%u\n", (unsigned)strlen(reply), (unsigned)longest);
        memset(big, 'a', sizeof big - 1u);
        memcpy(big, "OPEN ", 5);
        big[sizeof big - 1u] = '\0';
        CHECK(sel_ctl_exec(e, big, reply, sizeof reply, &rc) == 0);
        CHECK(rc == 10 && strncmp(reply, "overflow", 8) == 0);
        CHECK(sel_ctl_exec(e, "COMMANDS", small, sizeof small, &rc) == 0);
        CHECK(rc == 10 && strcmp(small, "result_truncated") == 0);
        /* the codec matches verbs exactly: lower case is not accepted, and the port adds no folding */
        CHECK(sel_ctl_exec(e, "status", reply, sizeof reply, &rc) == 0);
        CHECK(rc == 10 && strncmp(reply, "unknown_command", 15) == 0);
    } else if (strcmp(mode, "lasterror") == 0) {
        CHECK(sel_ctl_exec(e, "STATUS", reply, sizeof reply, &rc) == 1);
        CHECK(strlen(reply) >= 11u && strcmp(reply + strlen(reply) - 11u, "last_error=") == 0);
        CHECK(sel_ctl_exec(e, "FROBNICATE", reply, sizeof reply, &rc) == 0);
        CHECK(sel_ctl_exec(e, "STATUS", reply, sizeof reply, &rc) == 1);
        CHECK(strstr(reply, "last_error=unknown_command") != NULL);
    } else if (strcmp(mode, "quit") == 0) {
        CHECK(sel_engine_take_quit(e) == 0);
        CHECK(sel_ctl_exec(e, "QUIT", reply, sizeof reply, &rc) == 1 && rc == 0 && reply[0] == '\0');
        CHECK(sel_engine_take_quit(e) == 1);
        CHECK(sel_engine_take_quit(e) == 0);
        CHECK(sel_ctl_exec(e, "BEGIN", reply, sizeof reply, &rc) == 1);
        CHECK(sel_ctl_exec(e, "QUIT", reply, sizeof reply, &rc) == 1);
        CHECK(sel_engine_take_quit(e) == 0);
        CHECK(sel_ctl_exec(e, "END", reply, sizeof reply, &rc) == 1);
        CHECK(sel_engine_take_quit(e) == 1);
    } else if (strcmp(mode, "quitaction") == 0) {
        uint32_t i, rows = 0;
        char msg[200];
        const char* why;
        for (i = 0; i < sel_uim_action_count(); i++) {
            const sel_uim_action* a = sel_uim_action_at(i);
            if (strcmp(a->line, "QUIT") == 0) {
                rows++;
                CHECK(strcmp(a->menu, "File/Quit") == 0);
                CHECK(strcmp(a->label, "Quit") == 0);
            }
        }
        CHECK(rows == 1u);
        CHECK(sel_uim_actions_check(msg, sizeof msg) == 0);
        for (i = 0; i < sel_uim_exemption_count(); i++) {
            CHECK(strcmp(sel_uim_exemption_at(i, &why), "QUIT") != 0);
        }
    } else if (strcmp(mode, "raw") == 0) {
        raw_count c = {0, 0, 0};
        char pool[256];
        sel_engine_send_line(e, "COMMANDS", pool, sizeof pool);
        sel_engine_pump(e, raw_sink, &c);
        CHECK(c.results == SEL_CMD_COUNT);
        CHECK(c.intact == SEL_CMD_COUNT);
        CHECK(c.logs == 0);
    } else {
        unsigned lines = 0;
        char* p = reply;
        CHECK(sel_ctl_exec(e, "COMMANDS", reply, sizeof reply, &rc) == 1);
        CHECK(rc == 0);
        while (*p != '\0') {
            char* nl = strchr(p, '\n');
            if (nl != NULL) {
                *nl = '\0';
            }
            if (!line_ok(p)) {
                printf("bad line: %s\n", p);
                fails++;
            }
            lines++;
            if (nl == NULL) {
                break;
            }
            p = nl + 1;
        }
        CHECK(lines == SEL_CMD_COUNT);
    }
    sel_engine_destroy(e);
    if (fails == 0) {
        printf("ctl_exec: ok\n");
    }
    return fails == 0 ? 0 : 1;
}
