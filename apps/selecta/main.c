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

#include "ctlsock.h"
#include "integration_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHELL_RATE 44100u
#define SHELL_TURN_MS 20u
#define SHELL_RESAMPLER 1u /* linear */

/* ctx[0] = UI on_event hook, ctx[1] = UI state, ctx[2] = integration session. */
static void sink(void* ctx, const sel_message* evt) {
    sel_ui_on_event_fn on_event = (sel_ui_on_event_fn)((void**)ctx)[0];
    if (evt->id == SEL_EVT_ERROR) {
        fprintf(stderr, "error %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
    }
    if (on_event != NULL) {
        on_event(((void**)ctx)[1], evt);
    }
    sel_int_session_event((sel_int_session*)((void**)ctx)[2], evt);
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
    char pool[1024];
    if (sel_engine_send_line(e, line, pool, sizeof pool) == SEL_CODEC_OK) {
        sel_engine_pump(e, NULL, NULL); /* the pool is gone after this call */
    }
}

/* Control socket (T35): one line in, one reply out. A line that does not parse,
 * or whose command raises ERROR, is answered `ERROR <code> <detail>`; the rest
 * `OK`. Only OK lines count in socket_cmds. */
typedef struct ctl_ctx {
    sel_engine* engine;
    void** sink_ctx;
    char err[160];
} ctl_ctx;

static void ctl_sink(void* ctx, const sel_message* evt) {
    ctl_ctx* c = ctx;
    if (evt->id == SEL_EVT_ERROR && c->err[0] == '\0') {
        snprintf(c->err, sizeof c->err, "ERROR %s %s", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
    }
    sink(c->sink_ctx, evt);
}

static int ctl_handle(void* ctx, const char* line, char* reply, size_t cap) {
    ctl_ctx* c = ctx;
    char pool[1024];
    int rc;
    c->err[0] = '\0';
    rc = sel_engine_send_line(c->engine, line, pool, sizeof pool);
    sel_engine_pump(c->engine, ctl_sink, c); /* a rejected line queued its ERROR event */
    if (rc != SEL_CODEC_OK && c->err[0] == '\0') {
        snprintf(c->err, sizeof c->err, "ERROR %s", sel_codec_strerror(rc));
    }
    snprintf(reply, cap, "%s", c->err[0] != '\0' ? c->err : "OK");
    return c->err[0] == '\0';
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
    char cfgdir[256];
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
    void* ctx[3];
    char cwd[1024], cmd[1100];
    uint32_t waits = 0;
    int r;
    int has_keys = option(argc, argv, "--keys", NULL) != NULL;
#ifdef __APPLE__
    const char* default_int = "nowplaying";
#else
    const char* default_int = "mpris";
#endif
    const char* int_id = option(argc, argv, "--integration", (is_null || has_keys) ? "none" : default_int);
    sel_int_session* isess = NULL;
    int ctl_fd = -1;
    unsigned socket_cmds = 0;
    char ctl_path[512];
    ctl_ctx cc;

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
    cfg.config_dir = option(argc, argv, "--config-dir", NULL);
    if (cfg.config_dir == NULL && sel_config_dir(cfgdir, sizeof cfgdir)) {
        cfg.config_dir = cfgdir;
    }
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        fputs("selecta: engine allocation failed\n", stderr);
        return 1;
    }
    sel_engine_set_mix(e, SHELL_RATE, SHELL_RESAMPLER);
    sel_engine_set_output(e, oh);
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
    ctx[2] = NULL;

    cc.engine = e;
    cc.sink_ctx = ctx;
    cc.err[0] = '\0';
    if (!has_keys || getenv("SELECTA_SOCKET") != NULL) {
        sel_ctl_path(ctl_path, sizeof ctl_path);
        ctl_fd = sel_ctl_listen(ctl_path);
        if (ctl_fd == SEL_CTL_ALREADY_RUNNING) {
            fprintf(stderr, "selecta: already running (%s)\n", ctl_path);
            ui_close(ui);
            out_close(out);
            sel_engine_destroy(e);
            return 1;
        }
    }
    if (strcmp(int_id, "none") != 0) {
        isess = sel_int_session_open(e, host, int_id, sink, ctx);
        if (isess == NULL) {
            fprintf(stderr, "selecta: integration '%s' is not available\n", int_id);
        }
        ctx[2] = isess;
    }

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
        sel_int_session_poll(isess);
        if (ctl_fd >= 0) {
            socket_cmds += (unsigned)sel_ctl_serve(ctl_fd, is_null ? SHELL_TURN_MS : 0u, ctl_handle, &cc);
        }
    }

    ust = ui_stats != NULL ? ui_stats(ui) : NULL;
    sel_engine_stats_get(e, &est);
    printf("ui_waits=%u flushes=%u keys=%u\n", (unsigned)waits, (unsigned)sel_tag_find(ust, SEL_UI_STAT_FLUSHES, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_KEYS, 0));
    printf("songs_opened=%u queue_version=%u\n", (unsigned)est.songs_opened,
           (unsigned)sel_engine_state(e)->queue_version);

    printf("cmdline_runs=%u palette_opens=%u bindings=%u playlist_entries=%u undo_depth=%u redo_depth=%u view_style=%s\n",
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_CMDLINE_RUNS, 0), (unsigned)sel_tag_find(ust, SEL_UI_STAT_PALETTE_OPENS, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_BINDINGS, 0), (unsigned)sel_engine_state(e)->queue_count,
           (unsigned)est.undo_depth, (unsigned)est.redo_depth, sel_view_style_label(sel_engine_state(e)->view->style));

    printf("viz_frames=%u scope_reads=%u pattern_rows=%u mutes=%u dsp=crossfeed:%s fades=%u sleep_armed=%u\n",
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_VIZ_FRAMES, 0), (unsigned)sel_tag_find(ust, SEL_UI_STAT_SCOPE_READS, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_PATTERN_ROWS, 0), (unsigned)est.mutes,
           est.crossfeed_on ? "on" : "off", (unsigned)est.fades, (unsigned)est.sleep_armed);

    printf("engine_state pitch_cents=%d tempo_percent=%d mute_mask=%u mutes=%u\n", (int)sel_engine_state(e)->pitch_cents,
           (int)sel_engine_state(e)->tempo_percent, (unsigned)sel_engine_state(e)->mute_mask, (unsigned)est.mutes);

    if (ctl_fd >= 0) {
        printf("socket_cmds=%u\n", socket_cmds);
        sel_ctl_close(ctl_fd, ctl_path);
    }
    sel_int_session_close(isess);
    ui_close(ui);
    out_close(out);
    sel_engine_destroy(e);
    return 0;
}
