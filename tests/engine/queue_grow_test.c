/* I1373-2: the play queue starts small and grows on demand, with no fixed cap. On the 16 MB tier the queue used to be cut at 1024
 * entries / 128 KB of paths; this adds 3000 entries (more than that cut in count, and in text 3000 x ~60 bytes = 180 KB), checks
 * that none was refused (no ERROR queue_full), that every path is intact in order, that one UNDO of a REMOVE across the growth
 * restores the snapshot, and that a config-fixed capacity (queue_capacity) is still a hard cap. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"

#define N_ADD 3000u

static int g_fail, g_full;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_ERROR) g_full++;
}

static void run(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump_reads(eng, sink, NULL, 60000u);
}

static uint32_t qcount(sel_engine* e) { return sel_engine_state(e)->queue_count; }

static const char* qpath(sel_engine* e, uint32_t i) {
    const sel_state* st = sel_engine_state(e);
    return st->queue_text + st->queue[i].path_offset;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_qgrow_XXXXXX";
    char db[256], line[400], want[400];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_tag machine[2];
    uint32_t i, id0;
    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    machine[0].tag = SEL_MACHINE_FAST_RAM;
    machine[0].value = 16u * 1024u * 1024u;
    machine[1].tag = SEL_TAG_END;
    machine[1].value = 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.machine = machine;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);

    for (i = 0; i < N_ADD; i++) {
        snprintf(line, sizeof line, "QUEUE_ADD \"/nowhere/grow/test/directory/song_number_%05u.mod\"", (unsigned)i);
        run(e, line);
    }
    CHECK(g_full == 0);
    CHECK(qcount(e) == N_ADD);
    for (i = 0; i < qcount(e); i++) {
        snprintf(want, sizeof want, "/nowhere/grow/test/directory/song_number_%05u.mod", (unsigned)i);
        if (strcmp(qpath(e, i), want) != 0) {
            printf("FAIL: row %u is '%s'\n", (unsigned)i, qpath(e, i));
            g_fail++;
            break;
        }
    }

    /* UNDO still works on a queue that grew: remove the first row, undo it. */
    id0 = sel_engine_state(e)->queue[0].id;
    snprintf(line, sizeof line, "QUEUE_REMOVE %u", (unsigned)id0);
    run(e, line);
    CHECK(qcount(e) == N_ADD - 1u);
    run(e, "UNDO");
    CHECK(qcount(e) == N_ADD && strcmp(qpath(e, 0), "/nowhere/grow/test/directory/song_number_00000.mod") == 0);
    sel_engine_destroy(e);

    /* A caller-set capacity is a hard cap (embedded and tests rely on it). */
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.machine = machine;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.queue_capacity = 8u;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    g_full = 0;
    for (i = 0; i < 12u; i++) {
        snprintf(line, sizeof line, "QUEUE_ADD \"/nowhere/fixed/%u.mod\"", (unsigned)i);
        run(e, line);
    }
    CHECK(qcount(e) == 8u && g_full >= 1);
    sel_engine_destroy(e);

    snprintf(line, sizeof line, "rm -rf %s", dir);
    (void)system(line);
    if (g_fail) return 1;
    puts("queue_grow_ok");
    return 0;
}
