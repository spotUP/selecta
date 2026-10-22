/* selecta: the player shell (plan M1.1 phase 5, row I60). Creates the engine,
 * opens an output and a UI plugin, then loops: UI_WAIT, pump the output, pump
 * the engine, hand every event to the UI. The shell holds no playback logic
 * and no drawing.
 *
 *   selecta [--output <id>] [--ui <id>] [--start-dir <dir>]
 *           [--keys <file>] [--screen-dump <file>]
 *
 * --keys and --screen-dump are passed to the UI, which owns them. On exit it
 * prints `ui_waits=<w> flushes=<f> keys=<k>` and `songs_opened=<s>
 * queue_version=<q>` (the sentinels of test T19). */
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/output.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/ui.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHELL_RATE 44100u
#define SHELL_TURN_MS 20u
#define SHELL_RESAMPLER 1u /* linear */

static void sink(void* ctx, const sel_message* evt) {
    sel_ui_on_event_fn on_event = (sel_ui_on_event_fn)((void**)ctx)[0];
    if (evt->id == SEL_EVT_ERROR) {
        fprintf(stderr, "error %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
    }
    if (on_event != NULL) {
        on_event(((void**)ctx)[1], evt);
    }
}

static int g_mark_audio;

static uint32_t audio_pull(void* ctx, int16_t* stereo, uint32_t frames) {
    static int marked;
    if (g_mark_audio && !marked) { /* the null output pumps on the main thread */
        sel_engine_mark_audio_thread((sel_engine*)ctx);
        marked = 1;
    }
    return sel_engine_render((sel_engine*)ctx, stereo, frames);
}

static const sel_tag* hooks_of(uint32_t kind, const char* id) {
    const sel_tag* desc = sel_registry_find(kind, id);
    return desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
}

static const char* option(int argc, char** argv, const char* name, const char* def) {
    int i;
    for (i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return def;
}

static void send_line(sel_engine* e, const char* line) {
    sel_message m;
    char pool[1024];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) == SEL_CODEC_OK) {
        sel_engine_send(e, &m);
        sel_engine_pump(e, NULL, NULL); /* the pool is gone after this call */
    }
}

int main(int argc, char** argv) {
#ifdef __APPLE__
    const char* default_output = "coreaudio";
#else
    const char* default_output = "null";
#endif
    const sel_host* host = sel_host_default();
    const char* out_id = option(argc, argv, "--output", default_output);
    const char* ui_id = option(argc, argv, "--ui", "text");
    const char* start = option(argc, argv, "--start-dir", NULL);
    int is_null = strcmp(out_id, "null") == 0;
    const sel_tag* oh;
    const sel_tag* uh;
    sel_engine_config cfg;
    sel_engine* e;
    sel_out_open_fn out_open;
    sel_out_close_fn out_close;
    sel_out_set_source_fn out_set_source;
    sel_out_pump_fn out_pump;
    sel_ui_open_fn ui_open;
    sel_ui_wait_fn ui_wait;
    sel_ui_close_fn ui_close;
    sel_ui_stats_fn ui_stats;
    sel_ui_open_args args;
    sel_engine_stats est;
    const sel_tag* ust;
    void* out;
    void* ui;
    void* ctx[2];
    char cwd[1024], cmd[1100];
    uint32_t waits = 0;
    int r;

    sel_registry_init(host);
    oh = hooks_of(SEL_PLUGIN_KIND_OUTPUT, out_id);
    uh = hooks_of(SEL_PLUGIN_KIND_UI, ui_id);
    if (oh == NULL || uh == NULL) {
        fprintf(stderr, "selecta: %s '%s' is not registered\n", oh == NULL ? "output" : "ui",
                oh == NULL ? out_id : ui_id);
        return 2;
    }
    out_open = (sel_out_open_fn)sel_tag_find(oh, SEL_OUT_OPEN, 0);
    out_close = (sel_out_close_fn)sel_tag_find(oh, SEL_OUT_CLOSE, 0);
    out_set_source = (sel_out_set_source_fn)sel_tag_find(oh, SEL_OUT_SET_SOURCE, 0);
    out_pump = (sel_out_pump_fn)sel_tag_find(oh, SEL_OUT_PUMP, 0);
    ui_open = (sel_ui_open_fn)sel_tag_find(uh, SEL_UI_OPEN, 0);
    ctx[0] = (void*)sel_tag_find(uh, SEL_UI_ON_EVENT, 0);
    ui_wait = (sel_ui_wait_fn)sel_tag_find(uh, SEL_UI_WAIT, 0);
    ui_close = (sel_ui_close_fn)sel_tag_find(uh, SEL_UI_CLOSE, 0);
    ui_stats = (sel_ui_stats_fn)sel_tag_find(uh, SEL_UI_STATS, 0);
    if (!out_open || !out_close || !out_set_source || !out_pump || !ui_open || !ui_wait || !ui_close) {
        fputs("selecta: a plugin lacks a hook\n", stderr);
        return 2;
    }

    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(host); /* queue and browse buffers scale with the machine */
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        fputs("selecta: engine allocation failed\n", stderr);
        return 1;
    }
    sel_engine_set_mix(e, SHELL_RATE, SHELL_RESAMPLER);
    out = out_open(host, SHELL_RATE, 2);
    if (out == NULL) {
        fputs("selecta: no output device\n", stderr);
        sel_engine_destroy(e);
        return 77;
    }
    g_mark_audio = !is_null;
    out_set_source(out, audio_pull, e);

    memset(&args, 0, sizeof args);
    args.engine = e;
    args.host = host;
    args.argc = argc;
    args.argv = (const char* const*)argv;
    ui = ui_open(&args);
    if (ui == NULL) {
        fputs("selecta: the UI could not open (no terminal?)\n", stderr);
        out_close(out);
        sel_engine_destroy(e);
        return 1;
    }
    ctx[1] = ui;

    if (start == NULL) {
        start = getcwd(cwd, sizeof cwd);
    }
    if (start != NULL && start[0] == '/') {
        snprintf(cmd, sizeof cmd, "LIBRARY_BROWSE %s", start);
        send_line(e, cmd);
        send_line(e, "VIEW_SHOW browse");
    }
    sel_engine_pump(e, sink, ctx);

    /* The null output has no clock: each turn pumps exactly one UI_WAIT's worth
     * of frames, so a run is deterministic. A real output paces the loop. */
    while ((r = ui_wait(ui, is_null ? SHELL_TURN_MS : 0u)) >= 0) {
        waits++;
        out_pump(out, SHELL_RATE * SHELL_TURN_MS / 1000u);
        sel_engine_pump(e, sink, ctx);
    }

    ust = ui_stats != NULL ? ui_stats(ui) : NULL;
    sel_engine_stats_get(e, &est);
    printf("ui_waits=%u flushes=%u keys=%u\n", (unsigned)waits, (unsigned)sel_tag_find(ust, SEL_UI_STAT_FLUSHES, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_KEYS, 0));
    printf("songs_opened=%u queue_version=%u\n", (unsigned)est.songs_opened,
           (unsigned)sel_engine_state(e)->queue_version);

    ui_close(ui);
    out_close(out);
    sel_engine_destroy(e);
    return 0;
}
