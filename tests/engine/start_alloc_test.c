/* I1373: what sel_engine_create allocates at start on a 16 MB Amiga (FAST_RAM 16 MB, library engine). The search buckets used to be
 * allocated at the worst case (every searchable source, 40 full hits each) before the first search; they now grow on demand. The
 * test counts live bytes through a wrapping host and fails when the start need grows past the budget. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "db/db.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START_BUDGET (BUDGET_KB * 1024u) /* I1373: the engine start need on the 16 MB baseline; Selecta as a whole must start under 4 MB free */
#ifndef BUDGET_KB
#error BUDGET_KB is set by tests/CMakeLists.txt
#endif

static const sel_host* g_real;
static size_t g_live, g_peak;
static size_t g_big_n, g_big[64];

typedef struct hdr {
    size_t size;
    size_t pad;
} hdr;

static void* t_alloc(const sel_host* h, size_t size, uint32_t kind) {
    hdr* p = (hdr*)g_real->mem_alloc(g_real, sizeof(hdr) + size, kind);

    (void)h;
    if (p == NULL) return NULL;
    p->size = size;
    g_live += size;
    if (g_live > g_peak) g_peak = g_live;
    if (size >= 8u * 1024u && g_big_n < 64u) g_big[g_big_n++] = size;
    return p + 1;
}

static void t_free(const sel_host* h, void* ptr) {
    hdr* p = (hdr*)ptr - 1;

    (void)h;
    if (ptr == NULL) return;
    g_live -= p->size;
    g_real->mem_free(g_real, p);
}

int main(int argc, char** argv) {
    sel_host host;
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_tag machine[2];
    size_t base, need;
    unsigned i;

    if (argc < 2) return 2;
    g_real = sel_host_default();
    host = *g_real;
    host.mem_alloc = t_alloc;
    host.mem_free = t_free;
    sel_registry_init(g_real);
    lib = sel_library_open(g_real, sel_machine_probe(g_real), 0, argv[1]);
    if (lib == NULL) return 2;
    machine[0].tag = SEL_MACHINE_FAST_RAM;
    machine[0].value = 16u * 1024u * 1024u;
    machine[1].tag = SEL_TAG_END;
    machine[1].value = 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.machine = machine;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    base = g_live;
    g_peak = g_live;
    e = sel_engine_create(&host, &cfg);
    if (e == NULL) {
        puts("FAIL: engine create failed");
        return 1;
    }
    need = g_live - base;
    printf("start_alloc live=%lu peak=%lu bytes (budget %u)\n", (unsigned long)need, (unsigned long)(g_peak - base), START_BUDGET);
    for (i = 0; i < g_big_n; i++) printf("start_alloc block %lu\n", (unsigned long)g_big[i]);
    if (g_peak - base > START_BUDGET) {
        puts("FAIL: engine start allocates more than the 16 MB budget");
        return 1;
    }
    sel_engine_destroy(e);
    puts("start_alloc_ok");
    return 0;
}
