/* I710ga4: the engine's system-file contract (plan 2026-11-06-I710ga-system-files.md section 5), driven through the
 * engine's top level: OPEN, pump, SYSFILE_INSTALL, SYSFILE_RETRY. Two test-only replayers declare a SYSFILE each
 * (the probe module): "good.bin" (sha of the fixture good.bin, URL on mirror.sysfile.test) and "bad.bin" (same sha,
 * URL serving different bytes). RP_OPEN counts its calls and records host->config_get("sysfile.<name>"), so the
 * counters prove the new path ran: one request for two songs, a re-OPEN on land, the READY and FAILED events.
 * Local net fixtures only. argv: 1 scratch dir, 2 sha256 of fixtures good.bin, 3 db path, 4 fixture bad.bin path. */
#include "db/db.h"

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
#include <unistd.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static char g_sha[80];
static uint32_t g_opens[2];
static char g_seen_path[2][400];
static int g_state;

static int ends_with(const char* name, const char* ext) {
    size_t n = strlen(name), x = strlen(ext);
    return n >= x && strcmp(name + n - x, ext) == 0;
}

static void* probe_open(int which, const sel_host* host) {
    const char* p = host->config_get(host, which == 0 ? "sysfile.good.bin" : "sysfile.bad.bin", NULL);
    g_opens[which]++;
    snprintf(g_seen_path[which], sizeof g_seen_path[which], "%s", p != NULL ? p : "");
    return &g_state;
}

static void* open0(const sel_song_source* s, const sel_host* h, sel_rp_info* i) {
    (void)s;
    i->subsong_count = 1;
    i->default_subsong = 0;
    return probe_open(0, h);
}

static void* open1(const sel_song_source* s, const sel_host* h, sel_rp_info* i) {
    (void)s;
    i->subsong_count = 1;
    i->default_subsong = 0;
    return probe_open(1, h);
}

static void probe_close(void* st) {
    (void)st;
}

static int probe0(const uint8_t* d, size_t n, const char* name) {
    (void)d;
    (void)n;
    return name != NULL && ends_with(name, ".sfa") ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static int probe1(const uint8_t* d, size_t n, const char* name) {
    (void)d;
    (void)n;
    return name != NULL && ends_with(name, ".sfb") ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static int probe_start(void* st, uint32_t sub, uint32_t rate, const sel_voices* v) {
    (void)st;
    (void)sub;
    (void)rate;
    (void)v;
    return 1;
}

static uint32_t probe_render(void* st, int16_t* out, uint32_t frames) {
    (void)st;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

static sel_tag sf0[] = {{SEL_RP_SYSFILE_NAME, (uintptr_t) "good.bin"},
                        {SEL_RP_SYSFILE_LABEL, (uintptr_t) "Probe good file"},
                        {SEL_RP_SYSFILE_SHA256, (uintptr_t)g_sha},
                        {SEL_RP_SYSFILE_URLS, (uintptr_t) "http://mirror.sysfile.test/sysfiles/good.bin"},
                        {SEL_TAG_END, 0}};
static sel_tag sf1[] = {{SEL_RP_SYSFILE_NAME, (uintptr_t) "bad.bin"},
                        {SEL_RP_SYSFILE_LABEL, (uintptr_t) "Probe bad file"},
                        {SEL_RP_SYSFILE_SHA256, (uintptr_t)g_sha},
                        {SEL_RP_SYSFILE_URLS, (uintptr_t) "http://mirror.sysfile.test/sysfiles/bad.bin"},
                        {SEL_TAG_END, 0}};

#define PROBE(n)                                                               \
    static sel_tag hooks##n[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},            \
                                 {SEL_RP_NAME, (uintptr_t) "sfprobe" #n},      \
                                 {SEL_RP_PROBE, SEL_TAG_FN(probe##n)},         \
                                 {SEL_RP_OPEN, SEL_TAG_FN(open##n)},           \
                                 {SEL_RP_CLOSE, SEL_TAG_FN(probe_close)},      \
                                 {SEL_RP_START, SEL_TAG_FN(probe_start)},      \
                                 {SEL_RP_RENDER, SEL_TAG_FN(probe_render)},    \
                                 {SEL_RP_SYSFILE, (uintptr_t)sf##n},           \
                                 {SEL_TAG_END, 0}};                            \
    static sel_tag desc##n[] = {{SEL_PLUGIN_ID, (uintptr_t) "sfprobe" #n},     \
                                {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},   \
                                {SEL_PLUGIN_TAGS, (uintptr_t)hooks##n},        \
                                {SEL_TAG_END, 0}}
PROBE(0);
PROBE(1);

static uint32_t reg_count(void) {
    return 2u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0 ? desc0 : i == 1 ? desc1 : NULL;
}

static uint32_t g_ready, g_failed, g_missing;
static char g_last_missing[160], g_last_reason[200];

static void sink(void* ctx, const sel_message* m) {
    (void)ctx;
    if (m->id == SEL_EVT_SYSFILE_READY) g_ready++;
    if (m->id == SEL_EVT_SYSFILE_FAILED) {
        g_failed++;
        snprintf(g_last_reason, sizeof g_last_reason, "%s", m->args[1].v.s);
    }
    if (m->id == SEL_EVT_ERROR && strcmp(m->args[0].v.s, "system_file_missing") == 0) {
        g_missing++;
        snprintf(g_last_missing, sizeof g_last_missing, "%s", m->argc > 1 ? m->args[1].v.s : "");
    }
}

static void cmd_settle(sel_engine* e, const char* line, int settle) {
    sel_message m;
    char pool[1536];
    int t;

    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        printf("FAIL decode: %s\n", line);
        g_fail++;
        return;
    }
    sel_engine_send(e, &m);
    /* the engine thread takes the command asynchronously: pump about 300 ms so its events and counters settle */
    for (t = 0; t < settle; t++) {
        sel_engine_pump(e, sink, NULL);
        usleep(20000);
    }
}

static void cmd(sel_engine* e, const char* line) {
    cmd_settle(e, line, 15);
}

/* Pumps until `*counter` exceeds `from` (an event arrived) or about 10 s pass. */
static void wait_for(sel_engine* e, const uint32_t* counter, uint32_t from) {
    int t;

    for (t = 0; t < 500 && *counter <= from; t++) {
        sel_engine_pump(e, sink, NULL);
        usleep(20000);
    }
}

static void touch(const char* dir, const char* name, char* out, size_t cap) {
    FILE* f;

    snprintf(out, cap, "%s/%s", dir, name);
    f = fopen(out, "wb");
    if (f != NULL) {
        fputs("song", f);
        fclose(f);
    }
}

int main(int argc, char** argv) {
    sel_engine_config cfg;
    sel_engine* e;
    sel_engine_stats st;
    struct sel_library* lib;
    char cfgdir[512], a1[512], a2[512], b1[512], b2[512], line[1200], want[600];
    uint32_t failed0, missing0;

    if (argc < 5) {
        printf("usage: sysfile_engine_test dir sha db badbin\n");
        return 2;
    }
    snprintf(g_sha, sizeof g_sha, "%s", argv[2]);
    snprintf(cfgdir, sizeof cfgdir, "%s/cfg", argv[1]);
    CHECK(mkdir(cfgdir, 0777) == 0 || 1);
    touch(argv[1], "a1.sfa", a1, sizeof a1);
    touch(argv[1], "a2.sfa", a2, sizeof a2);
    touch(argv[1], "b1.sfb", b1, sizeof b1);
    touch(argv[1], "b2.sfb", b2, sizeof b2);
    lib = sel_library_open(sel_host_default(), sel_machine_probe(sel_host_default()), 0, argv[3]);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.config_dir = cfgdir;
    cfg.library = lib;
    e = sel_engine_create(sel_host_default(), &cfg);
    CHECK(e != NULL);

    /* missing file: the reason, no RP_OPEN, one request */
    snprintf(line, sizeof line, "OPEN %s", a1);
    cmd_settle(e, line, 0);

    /* the download lands: READY, then the song that waited is re-opened (RP_OPEN runs once, after the file is in) */
    wait_for(e, &g_ready, 0);
    CHECK(g_ready == 1);
    sel_engine_pump(e, sink, NULL);
    CHECK(g_missing == 1);
    CHECK(strcmp(g_last_missing, "sfprobe0:good.bin") == 0);
    sel_engine_stats_get(e, &st);
    CHECK(st.sysfile_fetches == 1);
    CHECK(st.sysfile_missing_reports == 1);
    CHECK(st.sysfile_installed == 1);
    CHECK(st.sysfile_reopens == 1);
    CHECK(g_opens[0] == 1);
    snprintf(want, sizeof want, "%s/good.bin", cfgdir);
    CHECK(strcmp(g_seen_path[0], want) == 0); /* config_get "sysfile.good.bin" is the stored file */
    CHECK(access(want, R_OK) == 0);
    CHECK(st.sysfile_fetches == 1);

    /* FAILED: the mirror serves other bytes; the reason is no_match */
    snprintf(line, sizeof line, "OPEN %s", b1);
    failed0 = g_failed;
    cmd(e, line);
    CHECK(g_opens[1] == 0 && strcmp(g_last_missing, "sfprobe1:bad.bin") == 0);
    wait_for(e, &g_failed, failed0);
    CHECK(g_failed == failed0 + 1);
    CHECK(strcmp(g_last_reason, "no_match") == 0);
    sel_engine_stats_get(e, &st);
    CHECK(st.sysfile_fetches == 2);

    /* FAILED does not re-request, also for a second song of the same plugin: reported again, no new download */
    cmd(e, line);
    snprintf(line, sizeof line, "OPEN %s", b2);
    cmd(e, line);
    sel_engine_stats_get(e, &st);
    CHECK(st.sysfile_fetches == 2 && st.sysfile_missing_reports == 4 && g_opens[1] == 0);

    /* RETRY re-requests */
    failed0 = g_failed;
    cmd(e, "SYSFILE_RETRY bad.bin");
    sel_engine_stats_get(e, &st);
    CHECK(st.sysfile_fetches == 3);
    wait_for(e, &g_failed, failed0);
    CHECK(g_failed == failed0 + 1);

    /* INSTALL with a wrong file keeps FAILED and gives the reason */
    failed0 = g_failed;
    snprintf(line, sizeof line, "SYSFILE_INSTALL bad.bin %s", argv[4]);
    cmd(e, line);
    CHECK(g_failed == failed0 + 1 && g_last_reason[0] != '\0');
    sel_engine_stats_get(e, &st);
    CHECK(st.sysfile_installed == 1 && g_opens[1] == 0);

    /* a later open of the same plugin finds the file: no report, no request. Last, because it takes the second slot
     * (nothing swaps slots without an audio device, so a third song could not start). */
    missing0 = g_missing;
    snprintf(line, sizeof line, "OPEN %s", a2);
    cmd(e, line);
    sel_engine_stats_get(e, &st);
    CHECK(g_opens[0] == 2 && g_missing == missing0 && st.sysfile_fetches == 3);

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("sysfile_engine_ok\n");
    return g_fail != 0;
}
