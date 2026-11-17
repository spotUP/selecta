/* I1440n4: cost of one sel_library_sticker_get per row (the collection page build looks up "deepsid.tags" per hvsc: row). */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ROWS 300 /* 5000 measured once: 160 ms single, 28 ms batched; 300 keeps the test under 1 s */

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char root[] = "/tmp/selecta_stickbench_XXXXXX";
    char dbp[320], key[64], tg[512];
    sel_library* lib;
    int i, hit = 0;
    double t0, ms;

    if (mkdtemp(root) == NULL) return 2;
    snprintf(dbp, sizeof dbp, "%s/lib.db", root);
    lib = sel_library_open(host, sel_machine_probe(host), 0, dbp);
    if (lib == NULL) return 1;
    for (i = 0; i < ROWS; i++) {
        snprintf(key, sizeof key, "hvsc:MUSICIANS/A/Artist_%d/Song_%d.sid", i / 20, i);
        if (i % 3 != 0) sel_library_sticker_set(lib, key, "deepsid.tags", "Arp, Cover, Vocal");
    }
    t0 = now_ms();
    for (i = 0; i < ROWS; i++) {
        snprintf(key, sizeof key, "hvsc:MUSICIANS/A/Artist_%d/Song_%d.sid", i / 20, i);
        hit += sel_library_sticker_get(lib, key, "deepsid.tags", tg, sizeof tg) != 0;
    }
    ms = now_ms() - t0;
    printf("sticker_get x%d: %.1f ms (%d hits)\n", ROWS, ms, hit);
    {
        char keys[32 * 96 + 1], vals[32 * SEL_STICKER_VALUE_MAX];
        int b, n, bhit = 0;
        t0 = now_ms();
        for (b = 0; b < ROWS; b += 32) {
            size_t used = 0;
            uint32_t got = 0;
            const char* v = vals;
            for (n = 0; n < 32 && b + n < ROWS; n++) {
                i = b + n;
                used += (size_t)snprintf(keys + used, sizeof keys - used, "hvsc:MUSICIANS/A/Artist_%d/Song_%d.sid", i / 20, i) + 1u;
            }
            keys[used] = '\0';
            if (!sel_library_sticker_get_many(lib, "deepsid.tags", keys, vals, sizeof vals, &got) || got != (uint32_t)n) return 4;
            for (i = 0; i < n; i++, v += strlen(v) + 1u) bhit += v[0] != '\0';
        }
        ms = now_ms() - t0;
        printf("sticker_get_many x%d: %.1f ms (%d hits)\n", ROWS, ms, bhit);
        if (bhit != hit) return 5;
    }
    printf("library_sticker_bench_ok\n");
    sel_library_close(lib);
    snprintf(dbp, sizeof dbp, "rm -rf %s", root);
    if (system(dbp) != 0) return 3;
    return 0;
}
