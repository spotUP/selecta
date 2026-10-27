/* M4-AX-0-1 ctl-exec-result and M4-AX-0-2 commands-reflection-format (plan 2026-10-25-m4-arexx): the shared reply
 * layer sel_ctl_exec returns rc and the RESULT lines of one protocol line.
 *   ctl_exec_test status    STATUS on a fresh engine: rc 0, state=stopped
 *   ctl_exec_test commands  every COMMANDS line is "VERB [arg ...]<TAB>Label<TAB>args=.. area=..", one per verb */
#include "ctlexec.h"
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>
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
