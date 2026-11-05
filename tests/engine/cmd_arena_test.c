/* I1082b: sel_engine_send copies a command's string arguments into its queue slot, so a caller's buffer is free the moment
 * the send returns. 64 sel_engine_send_line calls rotate through one 16-entry pool (each entry overwritten four times)
 * with no pump between; one pump then dispatches all of them. QUEUE_ADD puts each path in the queue: all 64 distinct
 * paths must be there, in order, intact (red before the fix: overwritten arguments). */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/protocol.h>

#include <stdio.h>
#include <string.h>

#define SENDS 64u
#define POOLS 16u

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

int main(void) {
    static char pools[POOLS][256];
    sel_engine_config cfg;
    sel_engine* e;
    const sel_state* st;
    char line[128], want[128];
    unsigned i, intact = 0, queued;

    memset(&cfg, 0, sizeof cfg);
    cfg.queue_capacity = 128u;
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL) {
        fputs("engine create failed\n", stderr);
        return 2;
    }
    for (i = 0; i < SENDS; i++) {
        snprintf(line, sizeof line, "QUEUE_ADD /nonexistent/cmd_arena_%02u.mod", i);
        if (sel_engine_send_line(e, line, pools[i % POOLS], sizeof pools[0]) != SEL_CODEC_OK) {
            printf("FAIL: send_line %u refused\n", i);
            return 1;
        }
    }
    sel_engine_pump(e, sink, NULL);
    st = sel_engine_state(e);
    for (i = 0; i < SENDS && i < st->queue_count; i++) {
        snprintf(want, sizeof want, "/nonexistent/cmd_arena_%02u.mod", i);
        intact += strcmp(st->queue_text + st->queue[i].path_offset, want) == 0;
    }
    queued = st->queue_count;
    printf("cmd_arena queued=%u intact=%u of %u\n", queued, intact, SENDS);
    sel_engine_destroy(e);
    if (intact != SENDS || queued != SENDS) {
        puts("FAIL: command arguments were overwritten before the pump");
        return 1;
    }
    puts("cmd_arena_ok");
    return 0;
}
