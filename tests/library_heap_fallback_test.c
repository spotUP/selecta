/* I227: the library opens when the machine's RAM is fragmented. On the Replay the D-M3-2 table asked
 * memsys5 for one 32 MB block while fast RAM had 50 MB free but only 16 MB contiguous, and Selecta
 * stopped with "cannot open the library database". The fake host below refuses any block above a cap,
 * the way a fragmented Amiga does, and the test checks the heap the library settles on. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include <stdio.h>
#include <stdlib.h>

static size_t g_cap;
static unsigned g_refused;
static sel_host g_host;

static void* capped_alloc(const sel_host* h, size_t size, uint32_t kind) {
    (void)h;
    (void)kind;
    if (size > g_cap) {
        g_refused++;
        return NULL;
    }
    return malloc(size);
}

static void plain_free(const sel_host* h, void* p) {
    (void)h;
    free(p);
}

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

int main(void) {
    const sel_tag* a060 = sel_machine_fake("a060"); /* 66 MB: the 8 MiB tier */
    sel_library* lib;

    g_host = *sel_host_default();
    g_host.mem_alloc = capped_alloc;
    g_host.mem_free = plain_free;
    check(sel_library_heap_bytes(a060) == 8u * 1024u * 1024u, "a060 tier is 8 MiB");

    /* Unfragmented: the full tier. */
    g_cap = (size_t)-1;
    lib = sel_library_open(&g_host, a060, 0, ":memory:");
    check(lib != NULL, "open with no cap");
    if (lib != NULL) {
        check(sel_library_heap_size(lib) == 8u * 1024u * 1024u, "full tier when the block exists");
        sel_library_close(lib);
    }

    /* Largest block 3 MB: the open settles on 2 MiB instead of failing. */
    g_cap = 3u * 1024u * 1024u;
    g_refused = 0;
    lib = sel_library_open(&g_host, a060, 0, ":memory:");
    check(lib != NULL, "open with the largest block below the tier");
    if (lib != NULL) {
        check(sel_library_heap_size(lib) == 2u * 1024u * 1024u, "heap halved to 2 MiB");
        check(g_refused == 2, "8 MiB and 4 MiB were asked first");
        sel_library_close(lib);
    }

    /* Below the 512 KiB floor: the open still fails cleanly. */
    g_cap = 256u * 1024u;
    lib = sel_library_open(&g_host, a060, 0, ":memory:");
    check(lib == NULL, "open fails below the floor");

    if (fails == 0) {
        puts("library_heap_fallback_ok");
    }
    return fails != 0;
}
