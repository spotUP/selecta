/* I912: a library result is bounded by its text bytes, not by rows, so a big catalogue can return more rows than the
 * queue capacity. VIEW_SOURCE query copied one metadata offset per result row into qmeta_off, which holds only
 * queue-capacity entries: a heap overflow (Johan's SIGSEGV with the real library.db, ASan WRITE in snapshot_meta).
 * Driven through the engine command entry with a guarded host allocator: every engine block carries a canary tail
 * that is checked when the engine frees it. 20 results against a queue capacity of 8 must leave every canary intact
 * and show 8 query rows. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"

#define CANARY 64u
#define HEAD 16u

static int g_fail;
static uint32_t g_smashed, g_checked;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void* guard_alloc(const sel_host* h, size_t size, uint32_t kind) {
    unsigned char* p = malloc(HEAD + size + CANARY);
    (void)h;
    (void)kind;
    if (p == NULL) return NULL;
    memcpy(p, &size, sizeof size);
    memset(p + HEAD, 0, size);
    memset(p + HEAD + size, 0xA5, CANARY);
    return p + HEAD;
}

static void guard_free(const sel_host* h, void* q) {
    unsigned char* p;
    size_t size, i;
    (void)h;
    if (q == NULL) return;
    p = (unsigned char*)q - HEAD;
    memcpy(&size, p, sizeof size);
    for (i = 0; i < CANARY; i++) {
        if (p[HEAD + size + i] != 0xA5) {
            g_smashed++;
            break;
        }
    }
    g_checked++;
    free(p);
}

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
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

int main(void) {
    static char tsv[8192];
    sel_host guarded = *sel_host_default();
    char dir[] = "/tmp/selecta_querymeta_XXXXXX";
    char db[256], line[256];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_lib_job job;
    uint32_t i;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(sel_host_default()) > 0);
    lib = sel_library_open(sel_host_default(), sel_machine_probe(sel_host_default()), 0, db);
    CHECK(lib != NULL);
    if (lib == NULL) return 1;
    tsv[0] = '\0';
    for (i = 0; i < 20u; i++) {
        snprintf(tsv + strlen(tsv), sizeof tsv - strlen(tsv), "/m/q%02u.mod\tlocal\t/m/q%02u.mod\tsong %u\tzedartist\tmod\t1994\n",
                 (unsigned)i, (unsigned)i, (unsigned)i);
    }
    memset(&job, 0, sizeof job);
    job.text = tsv;
    CHECK(sel_library_run(lib, SEL_LIB_REQ_IMPORT, &job) && job.count == 20u);

    guarded.mem_alloc = guard_alloc;
    guarded.mem_free = guard_free;
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.queue_capacity = 8u; /* fewer rows than the search returns: the text arena, not the row count, bounds the result */
    e = sel_engine_create(&guarded, &cfg);
    CHECK(e != NULL);
    if (e == NULL) return 1;

    run(e, "LIBRARY_SEARCH zedartist");
    run(e, "VIEW_SOURCE query");
    CHECK(sel_engine_state(e)->view->row_count == 8u); /* the query view holds a queue capacity of rows */

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(line, sizeof line, "rm -rf \"%s\"", dir);
    CHECK(system(line) == 0);
    CHECK(g_checked > 0u);
    CHECK(g_smashed == 0u);
    if (g_fail) {
        printf("query_meta_cap FAILED (%d checks, %u blocks smashed of %u)\n", g_fail, (unsigned)g_smashed, (unsigned)g_checked);
        return 1;
    }
    printf("query_meta_cap_ok\n");
    return 0;
}
