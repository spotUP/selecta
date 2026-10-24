/* Regression (I219a): the early prepare of the next song ran RP_START, and for a HW-kind replayer that start plays
 * at once and takes the hardware. A gapless prepare therefore cut a playing KPlayer song short (the next song took
 * Paula), and a HW-kind song queued after a mixed song closed the output while the mixed song still played.
 * Driven through the engine (QUEUE_ADD, OPTION, PLAY, pump and render) with a registry the test controls: a fake
 * HW-kind replayer counts its starts and releases, a fake RENDER replayer renders silence.
 * Sentinel: hw_starts and the fake's own start count prove the HW path ran, and the queue still advances to the
 * next song once the first one ended (max_song_ms).
 * Regression (I219d): while handed over, the pump clocked the HW song by local time, which the host reports in whole
 * seconds; song time stalled and then jumped a second, so the end policy fired early. The test host's local time has
 * that resolution and its clock_ms steps with drive(), so the song position after 400 ms tells the two clocks apart.
 * I219g (`seek` argument): SEEK on a HW song restarts it on the hardware and hands the target to the replayer's
 * RP_SEEK; the fake's seek call and its ms are the sentinel, seek_path 1 (native) and the song position the result. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static uint32_t g_hw_starts, g_hw_releases;
static uint32_t g_polls, g_loop_after; /* fake HW poll: calls so far, and the call that reports LOOPED (0 = never) */
static uint32_t g_seeks, g_seek_ms; /* fake HW seek: calls and the last target */
static int g_state; /* any non-NULL state will do: the fakes keep no per-song data */

static int ends_with(const char* name, const char* ext) {
    size_t n = strlen(name), x = strlen(ext);
    return n >= x && strcmp(name + n - x, ext) == 0;
}

static int hw_probe(const uint8_t* data, size_t size, const char* name) {
    (void)data;
    (void)size;
    return name != NULL && ends_with(name, ".hwm") ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static int rnd_probe(const uint8_t* data, size_t size, const char* name) {
    (void)data;
    (void)size;
    return name != NULL && ends_with(name, ".rnd") ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* fake_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    (void)src;
    (void)host;
    info->subsong_count = 1;
    info->default_subsong = 0;
    return &g_state;
}

static void fake_close(void* state) {
    (void)state;
}

static int hw_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    (void)state;
    (void)subsong;
    (void)rate;
    (void)voices;
    g_hw_starts++;
    return 1;
}

static void hw_release(void* state) {
    (void)state;
    g_hw_releases++;
}

static uint32_t hw_poll(void* state) {
    (void)state;
    g_polls++;
    return g_loop_after != 0u && g_polls == g_loop_after ? SEL_TICK_LOOPED : 0u;
}

static int hw_seek(void* state, uint32_t ms) {
    (void)state;
    g_seeks++;
    g_seek_ms = ms;
    return 1;
}

/* RP_POSITION of the fake HW player: order = the poll count, so a readout equal to the latest poll count proves the
 * engine sampled the position in its HW render loop, not only once when the song started (I219h). */
static int hw_position(void* state, sel_rp_position* out) {
    (void)state;
    out->order = g_polls;
    out->row = 7u;
    out->pattern = 3u;
    return 1;
}

static int rnd_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    (void)state;
    (void)subsong;
    (void)rate;
    (void)voices;
    return 1;
}

static uint32_t rnd_render(void* state, int16_t* out, uint32_t frames) {
    (void)state;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

static sel_tag g_hw_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_HW},
                               {SEL_RP_EXTENSIONS, (uintptr_t) "hwm"},
                               {SEL_RP_PROBE, SEL_TAG_FN(hw_probe)},
                               {SEL_RP_OPEN, SEL_TAG_FN(fake_open)},
                               {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},
                               {SEL_RP_START, SEL_TAG_FN(hw_start)},
                               {SEL_RP_HW_RELEASE, SEL_TAG_FN(hw_release)},
                               {SEL_RP_HW_POLL, SEL_TAG_FN(hw_poll)},
                               {SEL_RP_SEEK, SEL_TAG_FN(hw_seek)},
                               {SEL_RP_POSITION, SEL_TAG_FN(hw_position)},
                               {SEL_TAG_END, 0}};
static sel_tag g_rnd_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},
                                {SEL_RP_EXTENSIONS, (uintptr_t) "rnd"},
                                {SEL_RP_PROBE, SEL_TAG_FN(rnd_probe)},
                                {SEL_RP_OPEN, SEL_TAG_FN(fake_open)},
                                {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},
                                {SEL_RP_START, SEL_TAG_FN(rnd_start)},
                                {SEL_RP_RENDER, SEL_TAG_FN(rnd_render)},
                                {SEL_TAG_END, 0}};
static sel_tag g_hw_desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_hw"},
                              {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                              {SEL_PLUGIN_TAGS, (uintptr_t)g_hw_hooks},
                              {SEL_TAG_END, 0}};
static sel_tag g_rnd_desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_rnd"},
                               {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                               {SEL_PLUGIN_TAGS, (uintptr_t)g_rnd_hooks},
                               {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return 2u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0u ? g_hw_desc : (i == 1u ? g_rnd_desc : NULL);
}

static sel_engine* eng;

/* The test host keeps its own time: clock_ms counts the milliseconds drive() steps, and local time has the whole
 * second resolution of the real host (time() based), so a clock built on local time stalls and then jumps (I219d). */
static sel_host g_host;
static uint32_t g_ms;

static uint32_t test_clock_ms(const sel_host* h) {
    (void)h;
    return g_ms;
}

static int test_query(const sel_host* h, const sel_tag* ask, sel_tag* answer) {
    if (ask != NULL && ask->tag == SEL_HOSTQ_LOCAL_TIME) {
        answer->tag = SEL_HOSTQ_LOCAL_TIME;
        answer->value = 36000000u + g_ms / 1000u * 1000u;
        return 1;
    }
    return sel_host_default()->query(h, ask, answer);
}

static void run(const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
}

static void touch(const char* dir, const char* name) {
    char p[1024];
    FILE* f;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (f != NULL) {
        fputs("song", f);
        fclose(f);
    }
}

static void start(const char* dir, const char* first, const char* second) {
    sel_engine_config cfg;
    char line[1200];
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    g_host = *sel_host_default();
    g_host.clock_ms = test_clock_ms;
    g_host.query = test_query;
    g_ms = 0;
    eng = sel_engine_create(&g_host, &cfg);
    CHECK(eng != NULL);
    g_hw_starts = 0;
    g_hw_releases = 0;
    run("OPTION fade_ms 0");
    run("OPTION max_song_ms 1000"); /* inside the 2000 ms prepare lead from the first pump on */
    snprintf(line, sizeof line, "QUEUE_ADD %s/%s", dir, first);
    run(line);
    snprintf(line, sizeof line, "QUEUE_ADD %s/%s", dir, second);
    run(line);
    run("PLAY");
}

/* Pumps (and renders while the output is ours) for `ms` of test clock, or until `stop_at` HW starts. */
static void drive(uint32_t ms, uint32_t stop_at) {
    int16_t buf[441u * 2u];
    uint32_t t;
    for (t = 0; t < ms && g_hw_starts < stop_at; t += 10u) {
        if (!sel_engine_handed_over(eng)) {
            sel_engine_render(eng, buf, 441u); /* 10 ms at 44100 Hz, as an output would pull */
        }
        g_ms += 10u;
        sel_engine_pump(eng, NULL, NULL);
    }
}

int main(int argc, char** argv) {
    int loop_only = argc > 1 && strcmp(argv[1], "loop") == 0; /* ctest engine-hw-song-ends-on-loop */
    int seek_only = argc > 1 && strcmp(argv[1], "seek") == 0; /* ctest engine-hw-song-seeks-natively */
    char dir[] = "/tmp/selecta_hw_prepare_XXXXXX";
    sel_engine_stats st;
    CHECK(mkdtemp(dir) != NULL);
    touch(dir, "a.hwm");
    touch(dir, "b.hwm");
    touch(dir, "c.rnd");

    if (seek_only) {
        start(dir, "a.hwm", "b.hwm");
        run("OPTION max_song_ms 120000");
        drive(200u, 9u);
        CHECK(g_hw_starts == 1u);
        g_seeks = 0;
        run("SEEK 30000");
        drive(100u, 9u);
        CHECK(g_seeks == 1u);
        CHECK(g_seek_ms == 30000u);
        CHECK(g_hw_starts == 2u); /* the song restarted on the hardware before the seek */
        CHECK(sel_engine_handed_over(eng) == 1u);
        sel_engine_stats_get(eng, &st);
        CHECK(st.seek_path == 1u);
        CHECK(sel_engine_state(eng)->song.position_ms >= 30000u);
        CHECK(sel_engine_state(eng)->song.position_ms <= 30300u);
        {
            int16_t buf[441u * 2u];
            uint32_t i;
            for (i = 0; i < 3u; i++) { /* an output that keeps pulling while the hardware plays */
                sel_engine_render(eng, buf, 441u);
                sel_engine_pump(eng, NULL, NULL);
            }
        }
        CHECK(sel_engine_state(eng)->song.pattern_pos.order == g_polls); /* RP_POSITION on the HW path (I219h) */
        CHECK(sel_engine_state(eng)->song.pattern_pos.row == 7u);
        sel_engine_destroy(eng);
    }

    if (!loop_only && !seek_only) {
    /* HW song then HW song: the prepare window opens at once, yet the playing song keeps the hardware. */
    start(dir, "a.hwm", "b.hwm");
    drive(400u, 2u);
    CHECK(g_hw_starts == 1u);
    CHECK(g_hw_releases == 0u);
    CHECK(sel_engine_handed_over(eng) == 1u);
    /* The pump clocks the HW song by a millisecond clock: 400 ms of wall time is not a whole second of song (I219d). */
    CHECK(sel_engine_state(eng)->song.position_ms >= 300u);
    CHECK(sel_engine_state(eng)->song.position_ms <= 500u);
    /* Round trip: after max_song_ms the first song ends and the second one starts on the hardware. */
    drive(3000u, 2u);
    CHECK(g_hw_starts == 2u);
    CHECK(g_hw_releases >= 1u);
    sel_engine_stats_get(eng, &st);
    CHECK(st.hw_starts == 2u);
    sel_engine_destroy(eng);

    /* Mixed song then HW song: the output stays open until the mixed song ended, then the HW song takes over. */
    start(dir, "c.rnd", "b.hwm");
    drive(400u, 1u);
    CHECK(g_hw_starts == 0u);
    CHECK(sel_engine_handed_over(eng) == 0u);
    drive(3000u, 1u);
    CHECK(g_hw_starts == 1u);
    CHECK(sel_engine_handed_over(eng) == 1u);
    sel_engine_destroy(eng);
    }

    if (!seek_only) {
    /* Regression (I219b): HW slots never ticked, so a HW song ended only at max_song_ms. The fake's poll reports
     * LOOPED on its 40th call (about 40 render chunks, 256 frames each: ~230 ms); loops is 1 and the fade 0, so the
     * song ends then and the queue starts the next one, far before the 5000 ms cap. The poll count is the sentinel. */
    start(dir, "a.hwm", "b.hwm");
    run("OPTION max_song_ms 5000");
    g_polls = 0;
    g_loop_after = 40u;
    drive(1500u, 2u);
    CHECK(g_polls >= 40u);
    CHECK(g_hw_starts == 2u);
    CHECK(g_ms < 1200u); /* the second song started on the poll's LOOPED, not at the 5000 ms cap */
    g_loop_after = 0u;
    sel_engine_destroy(eng);
    }

    {
        char p[1024];
        snprintf(p, sizeof p, "%s/a.hwm", dir);
        remove(p);
        snprintf(p, sizeof p, "%s/b.hwm", dir);
        remove(p);
        snprintf(p, sizeof p, "%s/c.rnd", dir);
        remove(p);
        rmdir(dir);
    }
    if (g_fail) {
        printf("hw_prepare: %d failures\n", g_fail);
        return 1;
    }
    printf("hw_prepare: ok\n");
    return 0;
}
