/* M4-AX-3-1 / M4-AX-3-4 (plan 2026-10-25-m4-arexx section 5): the ARexx message mapping and its counters, driven
 * through the shared dispatcher sel_ctl_exec on a real engine.
 *   arexx_test reach  STATUS through arexx_dispatch: Result1 0, the reply string, arexx_cmds in the stats line,
 *                     and a sentinel proving the shared executor ran
 *   arexx_test rc     RC 10 for unknown verb, empty line, NULL, over-long line and IDLE; RC 20 at close;
 *                     no string without RXFF_RESULT
 *   arexx_test names  port name fallback SELECTA, SELECTA.2 .. SELECTA.99 */
#include "arexx.h"
#include "ctlexec.h"
#include <selecta/engine.h>
#include <selecta/host.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            printf("FAIL line %d: %s\n", __LINE__, #c);           \
            fails++;                                              \
        }                                                         \
    } while (0)

typedef struct {
    sel_engine* e;
    unsigned calls; /* sentinel: how often the shared executor ran */
} exec_ctx;

static int exec_shared(void* ctx, const char* line, char* reply, size_t cap, int* rc) {
    exec_ctx* x = ctx;
    x->calls++;
    return sel_ctl_exec(x->e, line, reply, cap, rc);
}

int main(int argc, char** argv) {
    sel_engine_config cfg;
    exec_ctx x;
    static char reply[SEL_CTL_REPLY_MAX];
    arexx_counters c = {0, 0};
    arexx_result r;
    char stats[128];
    char name[AREXX_NAME_MAX];
    char longline[AREXX_LINE_MAX + 8];
    const char* mode = argc > 1 ? argv[1] : "reach";

    memset(&cfg, 0, sizeof cfg);
    x.e = sel_engine_create(sel_host_default(), &cfg);
    x.calls = 0;
    CHECK(x.e != NULL);
    if (x.e == NULL) {
        return 1;
    }
    if (strcmp(mode, "reach") == 0) {
        arexx_dispatch("STATUS", 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(x.calls == 1);
        CHECK(r.result1 == 0);
        CHECK(r.has_string == 1);
        CHECK(strstr(r.text, "state=stopped") != NULL);
        CHECK(c.cmds == 1 && c.errors == 0);
        arexx_stats_line(stats, sizeof stats, "SELECTA", &c);
        CHECK(strstr(stats, "arexx_cmds=1") != NULL);
        CHECK(strstr(stats, "arexx_port=SELECTA") != NULL);
        arexx_dispatch("STATUS", 0, exec_shared, &x, reply, sizeof reply, &r, &c); /* no RXFF_RESULT: no string */
        CHECK(r.result1 == 0 && r.has_string == 0 && c.cmds == 2);
        arexx_stats_line(stats, sizeof stats, NULL, &c);
        CHECK(strcmp(stats, "arexx=off") == 0);
    } else if (strcmp(mode, "rc") == 0) {
        arexx_dispatch("FROBNICATE 1", 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(r.result1 == AREXX_RC_FAILED && r.has_string == 0 && strstr(r.text, "unknown_command") != NULL);
        arexx_dispatch("", 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(r.result1 == AREXX_RC_FAILED && strcmp(r.text, "empty_line") == 0);
        arexx_dispatch(NULL, 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(r.result1 == AREXX_RC_FAILED);
        memset(longline, 'A', sizeof longline - 1u);
        longline[sizeof longline - 1u] = '\0';
        arexx_dispatch(longline, 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(r.result1 == AREXX_RC_FAILED && strcmp(r.text, "overflow") == 0);
        CHECK(x.calls == 1); /* only the unknown verb reached the executor */
        arexx_dispatch("idle", 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(r.result1 == AREXX_RC_FAILED && strcmp(r.text, "idle_not_supported_over_arexx") == 0);
        arexx_dispatch("IDLE player", 1, exec_shared, &x, reply, sizeof reply, &r, &c);
        CHECK(r.result1 == AREXX_RC_FAILED && x.calls == 1);
        CHECK(c.cmds == 0 && c.errors == 6);
        arexx_closing_result(&r, &c);
        CHECK(r.result1 == AREXX_RC_CLOSING && strcmp(r.text, "port_closing") == 0 && c.errors == 7);
    } else if (strcmp(mode, "names") == 0) {
        unsigned n = 0;
        CHECK(arexx_port_name(0, name, sizeof name) && strcmp(name, "SELECTA") == 0);
        CHECK(arexx_port_name(1, name, sizeof name) && strcmp(name, "SELECTA.2") == 0);
        CHECK(arexx_port_name(98, name, sizeof name) && strcmp(name, "SELECTA.99") == 0);
        CHECK(!arexx_port_name(99, name, sizeof name));
        while (arexx_port_name(n, name, sizeof name)) {
            n++;
        }
        CHECK(n == 99u);
    } else {
        printf("unknown mode %s\n", mode);
        fails++;
    }
    sel_engine_destroy(x.e);
    if (fails == 0) {
        printf("arexx_test %s ok\n", mode);
    }
    return fails != 0;
}
