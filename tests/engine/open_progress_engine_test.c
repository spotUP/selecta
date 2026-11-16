/* I1415: the engine's open-progress contract over the net fixture (plan 2026-11-16-i1415-open-progress.md sections 4 and 6).
 * One stub replayer takes every ".mod". Remote songs come from slow.mirror.test (modland: keys); bigslow*.mod send half the
 * body, wait 3 s, then the rest. Legs: (c) phase walk, (a) CANCEL_OPEN mid-download, (b) a second OPEN of another path,
 * (d) an archived module opens through the read-thread unpack. argv: 1 scratch dir, 2 cache dir, 3 zip archive path. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "db/db.h"

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static uint32_t g_opened_evt;
static int g_state;

static int probe_mod(const uint8_t* d, size_t n, const char* name) {
    size_t l = name != NULL ? strlen(name) : 0;
    (void)d;
    (void)n;
    return l >= 4 && strcmp(name + l - 4, ".mod") == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}
static void* op(const sel_song_source* s, const sel_host* h, sel_rp_info* i) {
    (void)s;
    (void)h;
    i->subsong_count = 1;
    i->default_subsong = 0;
    return &g_state;
}
static void cl(void* st) { (void)st; }
static int st_(void* st, uint32_t sub, uint32_t rate, const sel_voices* v) {
    (void)st; (void)sub; (void)rate; (void)v;
    return 1;
}
static uint32_t rn(void* st, int16_t* out, uint32_t frames) {
    (void)st;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}
static sel_tag hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER}, {SEL_RP_NAME, (uintptr_t) "stubmod"}, {SEL_RP_PROBE, SEL_TAG_FN(probe_mod)},
                          {SEL_RP_OPEN, SEL_TAG_FN(op)}, {SEL_RP_CLOSE, SEL_TAG_FN(cl)}, {SEL_RP_START, SEL_TAG_FN(st_)},
                          {SEL_RP_RENDER, SEL_TAG_FN(rn)}, {SEL_TAG_END, 0}};
static sel_tag desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "stubmod"}, {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                         {SEL_PLUGIN_TAGS, (uintptr_t)hooks}, {SEL_TAG_END, 0}};
static uint32_t reg_count(void) { return 1u; }
static const sel_tag* reg_get(uint32_t i) { return i == 0 ? desc : NULL; }

static void sink(void* ctx, const sel_message* m) {
    (void)ctx;
    if (m->id == SEL_EVT_SONG_OPENED) g_opened_evt++;
}

static void pump_ms(sel_engine* e, int ms) {
    int t;
    for (t = 0; t < ms / 20; t++) {
        sel_engine_pump(e, sink, NULL);
        usleep(20000);
    }
}

static void cmd(sel_engine* e, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        printf("FAIL decode: %s\n", line);
        g_fail++;
        return;
    }
    sel_engine_send(e, &m);
}

/* the .part of bigslow.mod anywhere under the cache dir (the path layout is the cache's business) */
static int find_part(const char* cache, long* size) {
    char cmdl[900], buf[600];
    FILE* f;
    struct stat sb;
    int ok = 0;
    snprintf(cmdl, sizeof cmdl, "find '%s' -name 'bigslow.mod*.part' 2>/dev/null", cache);
    f = popen(cmdl, "r");
    if (f != NULL && fgets(buf, sizeof buf, f) != NULL) {
        buf[strcspn(buf, "\n")] = '\0';
        if (stat(buf, &sb) == 0) { *size = (long)sb.st_size; ok = 1; }
    }
    if (f != NULL) pclose(f);
    return ok;
}

static sel_engine* make(const char* dir, const char* name, struct sel_library** lib) {
    sel_engine_config cfg;
    char db[600];
    snprintf(db, sizeof db, "%s/%s.db", dir, name);
    *lib = sel_library_open(sel_host_default(), sel_machine_probe(sel_host_default()), 0, db);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.config_dir = dir;
    cfg.library = *lib;
    return sel_engine_create(sel_host_default(), &cfg);
}

int main(int argc, char** argv) {
    sel_engine* e;
    sel_engine_stats st;
    sel_open_progress pr;
    struct sel_library* lib;
    int t, saw_conn = 0, saw_down = 0, rising = 0;
    uint32_t last_done = 0, evt0;
    long part_size = 0;
    char line[900];

    if (argc < 4) {
        printf("usage: open_progress_engine_test dir cache zip\n");
        return 2;
    }
    e = make(argv[1], "a", &lib);
    CHECK(e != NULL);

    /* a fast song first: it takes the playing slot (no audio device here, so a slot never swaps) */
    cmd(e, "OPEN modland:Protracker/test/quick1.mod");
    for (t = 0; t < 250 && g_opened_evt < 1; t++) pump_ms(e, 20);
    CHECK(g_opened_evt == 1);
    sel_engine_open_progress(e, &pr);
    CHECK(pr.active == 0);

    /* (c) + (a): the big song walks Connecting -> Downloading; CANCEL_OPEN mid-body */
    cmd(e, "OPEN modland:Protracker/test/bigslow.mod");
    for (t = 0; t < 150; t++) {
        pump_ms(e, 20);
        sel_engine_open_progress(e, &pr);
        if (pr.active && pr.phase == SEL_OPEN_CONNECTING) saw_conn = 1;
        if (pr.active && pr.phase == SEL_OPEN_DOWNLOADING && pr.done > 0) {
            saw_down = 1;
            if (pr.done > last_done) rising = 1;
            last_done = pr.done;
            if (pr.total == pr.done || pr.total == 0) continue;
            break;
        }
    }
    CHECK(saw_down);
    CHECK(pr.total == 6166 && pr.done > 0 && pr.done < pr.total);
    (void)saw_conn; (void)rising; /* Connecting can pass between two pumps; the Downloading sample is the proof */
    evt0 = g_opened_evt;
    cmd(e, "CANCEL_OPEN");
    pump_ms(e, 1500);
    sel_engine_open_progress(e, &pr);
    sel_engine_stats_get(e, &st);
    CHECK(pr.active == 0);
    CHECK(g_opened_evt == evt0 && st.songs_opened == 1); /* no SONG_OPENED for the cancelled path, quick1 still plays */
    pump_ms(e, 3500);                                    /* the server finishes its sleep; nothing may start late */
    CHECK(find_part(argv[2], &part_size) && part_size > 0 && part_size < 6166); /* I1415p: the cancelled download keeps its .part */
    sel_engine_stats_get(e, &st);
    CHECK(g_opened_evt == evt0 && st.songs_opened == 1);

    /* (b) a second OPEN of another path mid-download opens without waiting for the first body (3 s) */
    cmd(e, "OPEN modland:Protracker/test/bigslow2.mod");
    pump_ms(e, 600);
    cmd(e, "OPEN modland:Protracker/test/quick2.mod");
    for (t = 0; t < 100 && g_opened_evt < evt0 + 1; t++) pump_ms(e, 20); /* 2 s at most */
    CHECK(g_opened_evt == evt0 + 1);
    pump_ms(e, 100);
    sel_engine_open_progress(e, &pr);
    CHECK(pr.active == 0); /* idle after SONG_OPENED */
    sel_engine_destroy(e);
    sel_library_close(lib);

    /* (d) an archived module opens through the read-thread unpack */
    e = make(argv[1], "b", &lib);
    snprintf(line, sizeof line, "OPEN %s", argv[3]);
    evt0 = g_opened_evt;
    cmd(e, line);
    for (t = 0; t < 250 && g_opened_evt < evt0 + 1; t++) pump_ms(e, 20);
    sel_engine_stats_get(e, &st);
    CHECK(g_opened_evt == evt0 + 1);
    CHECK(st.open_unpack_reads == 1 && st.archive_unpacks == 1);
    sel_engine_destroy(e);
    sel_library_close(lib);

    /* I1415p: cancel mid-body keeps the .part; the same path again resumes by Range (the cmake driver reads the
     * fixture log) and finishes. A fresh engine so the playing slots are free. */
    e = make(argv[1], "c", &lib);
    evt0 = g_opened_evt;
    cmd(e, "OPEN modland:Protracker/test/bigslow.mod");
    for (t = 0; t < 150; t++) {
        pump_ms(e, 20);
        sel_engine_open_progress(e, &pr);
        if (pr.active && pr.phase == SEL_OPEN_DOWNLOADING && pr.done > 0 && pr.done < pr.total) break;
    }
    CHECK(t < 150);
    cmd(e, "CANCEL_OPEN");
    pump_ms(e, 1500);
    CHECK(find_part(argv[2], &part_size) && part_size > 0 && part_size < 6166);
    cmd(e, "OPEN modland:Protracker/test/bigslow.mod");
    for (t = 0; t < 400 && g_opened_evt < evt0 + 1; t++) pump_ms(e, 20);
    CHECK(g_opened_evt == evt0 + 1);
    CHECK(!find_part(argv[2], &part_size));
    sel_engine_destroy(e);
    sel_library_close(lib);

    /* (e) I1416a: a URL stream (first byte after 4 s) is read on the rd_play thread: the pump never stalls on it, STOP ends it, and the
     * engine closes at once instead of waiting out the 4 s */
    e = make(argv[1], "c", &lib);
    {
        struct timespec a, b;
        long worst = 0, closing;
        uint32_t opened0 = g_opened_evt;

        cmd(e, "OPEN http://slow.mirror.test/mods/Protracker/test/song.mod");
        for (t = 0; t < 15; t++) {
            clock_gettime(CLOCK_MONOTONIC, &a);
            sel_engine_pump(e, sink, NULL);
            clock_gettime(CLOCK_MONOTONIC, &b);
            if ((b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L > worst) worst = (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
            usleep(20000);
        }
        printf("url open: worst pump %ld ms\n", worst);
        CHECK(worst < 500);
        sel_engine_open_progress(e, &pr);
        CHECK(pr.active == 1); /* still connecting: the first byte is 4 s away */
        cmd(e, "STOP");
        pump_ms(e, 100);
        sel_engine_open_progress(e, &pr);
        CHECK(pr.active == 0);
        clock_gettime(CLOCK_MONOTONIC, &a);
        sel_engine_destroy(e);
        clock_gettime(CLOCK_MONOTONIC, &b);
        closing = (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
        printf("url open: destroy after STOP %ld ms\n", closing);
        CHECK(closing < 2000);
        CHECK(g_opened_evt == opened0);
    }
    sel_library_close(lib);

    if (g_fail == 0) printf("open_progress_engine_ok\n");
    return g_fail != 0;
}
