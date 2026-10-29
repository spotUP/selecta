/* selecta: the player shell (plan M1.1 phase 5, row I60). Creates the engine,
 * opens an output and a UI plugin, then loops: UI_WAIT, pump the output, pump
 * the engine, hand every event to the UI. The shell holds no playback logic
 * and no drawing.
 *
 *   selecta [--output <id>] [--ui <id>] [--start-dir <dir>]
 *           [--keys <file>] [--screen-dump <file>] [--catalogue <tsv>] [--exec "<command>"]...
 *
 * --catalogue backs the engine with an in-memory library imported from a TSV; each --exec
 * line is sent as a command once the engine is up (T66 uses LIBRARY_SEARCH and VIEW_SOURCE).
 * A positional share link (selecta://... or the DEViLBOX form) is sent as OPEN (T84).
 * *
 * --keys and --screen-dump are passed to the UI, which owns them. On exit it
 * prints `ui_waits=<w> flushes=<f> keys=<k>` and `songs_opened=<s>
 * queue_version=<q>` (the sentinels of test T19). */
#ifdef __amigaos__
#include "amiga_start.h"
#include "startup_error.h"
#endif
#include "shell_pace.h"
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/output.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/ui.h>

#ifndef SEL_NO_CONTROL_SOCKET
#include "ctlsock.h"
#endif
#include "ctlexec.h"
#ifdef __amigaos__
#include "arexx.h"
#endif
#include "catalogue.h"
#include "startup_error.h"
#include "console_fallback.h"
#include "integration_host.h"
#include "db.h"
#include "local.h"
#include "share.h"
#include "../../src/engine/options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHELL_TURN_MS 20u
#define SHELL_RESAMPLER 1u /* linear */

/* The open UIs (DUI-P1-2): every `--ui <id>` opens one, each sees every event and takes its turn in the wait loop.
 * Slot 0 is the primary UI; its stats fill the exit lines and UI_SWITCH replaces it (DUI-P1-1). */
#define SHELL_MAX_UIS 4
typedef struct shell_ui {
    const char* id;
    void* state;
    sel_ui_open_fn open;
    sel_ui_on_event_fn on_event;
    sel_ui_wait_fn wait;
    sel_ui_close_fn close;
    sel_ui_stats_fn stats;
    uint32_t view_styles; /* SEL_UI_VIEW_STYLES: sel_view_style bits the UI offers, 0 the classic three */
    int (*theme)(void* state, const char* path); /* SEL_UI_THEME (I676b), NULL when the UI has none */
} shell_ui;

static shell_ui g_uis[SHELL_MAX_UIS];
static unsigned g_ui_count;
static int g_trace; /* --stats: one stderr line per launch phase (I524b) */
static void sel_trace(const sel_host* h, const char* phase) {
    if (g_trace) {
        fprintf(stderr, "trace t_ms=%u %s\n", (unsigned)h->clock_ms(h), phase);
    }
}
/* Every fatal start-up exit reports through sel_startup_error (stderr, plus a requester on the Amiga without a
 * console, I563); --stats prints the report count so a test can prove the shared path ran. */
#define STARTUP_FAIL(...)                                                                                          \
    do {                                                                                                           \
        unsigned reports_ = sel_startup_error(__VA_ARGS__);                                                        \
        if (g_trace) {                                                                                             \
            fprintf(stderr, "startup_error_reports=%u\n", reports_);                                               \
        }                                                                                                          \
    } while (0)
static int g_stats; /* --stats: the shell reports what the UIs and prefs did on stdout */

/* ctx[0] = engine (set once it exists), ctx[1] = primary UI state, ctx[2] = integration session. */
static void sink(void* ctx, const sel_message* evt) {
    unsigned u;
    if (evt->id == SEL_EVT_ERROR) {
        fprintf(stderr, "error %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
    }
    if (g_stats && evt->id == SEL_EVT_PREFS_CHANGED && evt->argc > 1) { /* the reply to PREF_GET and PREF_SET */
        printf("pref %s=%s\n", evt->args[0].v.s, evt->args[1].v.s);
    }
    for (u = 0; u < g_ui_count; u++) {
        if (g_uis[u].on_event != NULL) {
            g_uis[u].on_event(g_uis[u].state, evt);
        }
    }
    sel_int_session_event((sel_int_session*)((void**)ctx)[2], evt);
#ifdef __amigaos__
    if (evt->id == SEL_EVT_SONG_OPENED && ((void**)ctx)[0] != NULL) { /* hook_command (Amiga only, open question 9): one launch per opened song */
        static arexx_hook_stats hook_stats;
        char logline[160];
        const char* script = sel_engine_state((sel_engine*)((void**)ctx)[0])->options[SEL_OPT_HOOK_COMMAND];
        if (arexx_hook_event(1, script, arexx_hook_launch, NULL, &hook_stats, logline, sizeof logline) == 0) {
            fprintf(stderr, "log %s\n", logline);
        }
    }
#endif
}

static int g_mark_audio;
static int g_marked; /* the output's pull thread is marked as the audio thread */
static volatile int g_pumping; /* the shell is inside so.pump: a pull now runs on the main thread, never the audio thread (I684c) */

static uint32_t audio_pull(void* ctx, int16_t* stereo, uint32_t frames) {
    if (g_mark_audio && !g_marked && !g_pumping) { /* a clockless output pumps on the main thread */
        sel_engine_mark_audio_thread((sel_engine*)ctx);
        g_marked = 1;
    }
    return sel_engine_render((sel_engine*)ctx, stereo, frames);
}

/* Output handover (I219): a HW-kind replayer (KPlayer) drives Paula itself, so the output closes while it plays
 * and reopens before the next song that needs it. */
typedef struct shell_output {
    sel_out_open_fn open;
    sel_out_close_fn close;
    sel_out_set_source_fn set_source;
    sel_out_pump_fn pump;
    const sel_tag* machine;
    const sel_host* host;
    sel_engine* engine;
    uint32_t rate; /* sel_registry_output_rate: the one rate for set_mix, open and the turn length */
    void* out; /* NULL while handed over */
    int paced; /* 0: the output's pump never blocks (SEL_OUT_UNPACED), so the UI wait paces the turn (I684c) */
} shell_output;

static int shell_handover(void* ctx, uint32_t release) {
    shell_output* so = (shell_output*)ctx;
    if (release) {
        if (so->out != NULL) {
            so->close(so->out);
            so->out = NULL;
        }
        return 1;
    }
    if (so->out == NULL) {
        so->out = so->open(so->host, so->rate, 2);
        if (so->out == NULL) {
            return 0;
        }
        g_marked = 0; /* a new pull thread marks itself on its first call */
        so->set_source(so->out, audio_pull, so->engine);
    }
    return 1; /* so->paced is the same output's: a reopen keeps it */
}

static const sel_tag* hooks_of(uint32_t kind, const char* id) {
    const sel_tag* desc = sel_registry_find(kind, id);
    return desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
}

/* Output swap (I532): the engine routes a PCM song while the open output is voice-only (Paula discards PCM), so it
 * asks for a PCM sink by id. Close the open output, open `id` at its own rate, and point it at the engine. */
static int shell_switch_output(void* ctx, const char* id) {
    shell_output* so = (shell_output*)ctx;
    const sel_tag* oh = hooks_of(SEL_PLUGIN_KIND_OUTPUT, id);
    sel_out_open_fn open_fn;
    sel_out_close_fn close_fn;
    sel_out_set_source_fn source_fn;
    sel_out_pump_fn pump_fn;
    uint32_t rate;
    void* out;
    if (oh == NULL) {
        return 0;
    }
    open_fn = (sel_out_open_fn)sel_tag_find(oh, SEL_OUT_OPEN, 0);
    close_fn = (sel_out_close_fn)sel_tag_find(oh, SEL_OUT_CLOSE, 0);
    source_fn = (sel_out_set_source_fn)sel_tag_find(oh, SEL_OUT_SET_SOURCE, 0);
    pump_fn = (sel_out_pump_fn)sel_tag_find(oh, SEL_OUT_PUMP, 0);
    if (!open_fn || !close_fn || !source_fn || !pump_fn) {
        return 0;
    }
    if (so->out != NULL) {
        so->close(so->out);
        so->out = NULL;
    }
    rate = sel_registry_output_rate(oh, so->machine);
    sel_engine_set_mix(so->engine, rate, SHELL_RESAMPLER);
    out = open_fn(so->host, rate, 2);
    if (out == NULL) {
        return 0;
    }
    g_marked = 0; /* a new pull thread marks itself on its first call */
    source_fn(out, audio_pull, so->engine);
    so->rate = rate;
    so->open = open_fn;
    so->close = close_fn;
    so->set_source = source_fn;
    so->pump = pump_fn;
    so->paced = sel_tag_find(oh, SEL_OUT_UNPACED, 0) == 0;
    so->out = out;
    return 1;
}

/* Default library DB: sel_default_db_path (src/core/config_dir.c), shared with selecta-cli. */
static const char* default_db_path(char* out, size_t cap) {
    return sel_default_db_path(out, cap);
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

/* Fill a slot from the registry; 0 when the id is no UI or lacks a hook. */
static int ui_resolve(const char* id, shell_ui* u) {
    const sel_tag* desc = sel_registry_find(SEL_PLUGIN_KIND_UI, id);
    const sel_tag* h = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    if (h == NULL) {
        return 0;
    }
    /* The registry's own id string: `id` may be a caller's stack buffer (the UI_SWITCH name) that is gone by the next
     * stats line, which printed an empty or garbage id (I521). */
    u->id = (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, 0);
    u->open = (sel_ui_open_fn)sel_tag_find(h, SEL_UI_OPEN, 0);
    u->on_event = (sel_ui_on_event_fn)sel_tag_find(h, SEL_UI_ON_EVENT, 0);
    u->wait = (sel_ui_wait_fn)sel_tag_find(h, SEL_UI_WAIT, 0);
    u->close = (sel_ui_close_fn)sel_tag_find(h, SEL_UI_CLOSE, 0);
    u->stats = (sel_ui_stats_fn)sel_tag_find(h, SEL_UI_STATS, 0);
    u->view_styles = (uint32_t)sel_tag_find(h, SEL_UI_VIEW_STYLES, 0);
    u->theme = (int (*)(void*, const char*))sel_tag_find(h, SEL_UI_THEME, 0);
    return u->open != NULL && u->wait != NULL && u->close != NULL;
}

static const sel_tag* ui_stats_of(const shell_ui* u) {
    return u->stats != NULL && u->state != NULL ? u->stats(u->state) : NULL;
}

/* A 64-bit golden hash stat: 16 hex digits, the zero hash when the UI does not report it. */
static const char* stat_hash_str(const sel_tag* stats, uint32_t tag) {
    const char* v = (const char*)sel_tag_find(stats, tag, 0);
    return v != NULL ? v : "0000000000000000";
}

static void send_line(sel_engine* e, const char* line);

/* I674: a positional song path (`selecta --ui mui RAM:meme.mod`). The shell owns it for every UI: it is a plain argument
 * (no leading '-', no KEY=VALUE option word, not a share link, not the value of a `--name value` option) naming a file. */
static int is_song_argument(int argc, char** argv, int a) {
    static const char* const flags[] = {"--stats", "--tokens", "--actions"};
    struct stat sb;
    size_t k;
    (void)argc;
    if (argv[a][0] == '-' || sel_share_is_link(argv[a]) || (strchr(argv[a], '=') != NULL && strpbrk(argv[a], "/:") == NULL)) {
        return 0;
    }
    if (a > 1 && strncmp(argv[a - 1], "--", 2) == 0) {
        for (k = 0; k < sizeof flags / sizeof flags[0]; k++) {
            if (strcmp(argv[a - 1], flags[k]) == 0) {
                break;
            }
        }
        if (k == sizeof flags / sizeof flags[0]) {
            return 0; /* the value of an option */
        }
    }
    return stat(argv[a], &sb) == 0 && S_ISREG(sb.st_mode);
}

/* Start options as KEY=VALUE words (MODE=mini, DUI-P1-3): the one parser (src/engine/options.c, the same as
 * ToolTypes and selecta-cli) validates each word against a scratch prefs table, and the prefs it changed reach the
 * engine as PREF_SET, so the engine and every UI see them as an ordinary pref change. */
static void apply_option_words(sel_engine* e, int argc, char** argv) {
    sel_prefs defaults;
    sel_prefs set;
    sel_opts o;
    int a, id;
    char cmd[300];
    sel_prefs_init(&defaults);
    for (a = 1; a < argc; a++) {
        int r;
        if (argv[a][0] == '-' || strchr(argv[a], '=') == NULL || strpbrk(argv[a], "/:") != NULL) {
            continue;
        }
        sel_prefs_init(&set);
        memset(&o, 0, sizeof o);
        r = sel_opts_apply_word(&set, &o, argv[a]);
        if (r == SEL_OPTS_BAD_VALUE) {
            STARTUP_FAIL("bad option value in '%s'", argv[a]);
        }
        for (id = 0; r == SEL_OPTS_OK && id < SEL_PREF_COUNT; id++) {
            if (strcmp(set.value[id], defaults.value[id]) != 0) {
                snprintf(cmd, sizeof cmd, "PREF_SET %s %s", sel_prefs_key(id), set.value[id]);
                send_line(e, cmd);
            }
        }
    }
}

/* UI_SWITCH <id> (DUI-P1-1): the primary UI closes and the named one opens in its place. The engine, the queue and
 * the playing song are untouched, so playback continues across the switch. A closed UI's frame count is reported
 * as `ui_closed` because its state is gone at exit. Returns 1 switched, 0 unknown id (nothing changed), -1 the new
 * UI could not open. */
static int ui_switch(const char* id, const sel_ui_open_args* args, void** ctx) {
    shell_ui next;
    shell_ui* cur = &g_uis[0];
    memset(&next, 0, sizeof next);
    if (!ui_resolve(id, &next)) {
        fprintf(stderr, "error ui_unknown %s\n", id);
        return 0;
    }
    printf("ui_closed id=%s frames=%u\n", cur->id, (unsigned)sel_tag_find(ui_stats_of(cur), SEL_UI_STAT_FRAMES, 0));
    cur->close(cur->state);
    cur->state = NULL; /* no event reaches a closed UI while the next one opens */
    next.state = next.open(args);
    if (next.state == NULL) {
        STARTUP_FAIL("the UI '%s' could not open", id);
        return -1;
    }
    *cur = next;
    sel_engine_set_ui_view_styles((sel_engine*)ctx[0], next.view_styles);
    ctx[1] = next.state;
    return 1;
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
#if !defined(SEL_NO_CONTROL_SOCKET) || defined(__amigaos__)
typedef struct ctl_ctx {
    sel_engine* engine;
    void** sink_ctx;
} ctl_ctx;

static void ctl_fwd(void* ctx, const sel_message* evt) {
    sink(((ctl_ctx*)ctx)->sink_ctx, evt);
}

#endif
#ifdef __amigaos__
/* ARexx executor: the same shared dispatcher as the socket, so every UI still sees the events (M4-AX-3-2). */
static int arexx_exec(void* ctx, const char* line, char* reply, size_t cap, int* rc) {
    ctl_ctx* c = ctx;
    return sel_ctl_exec_fwd(c->engine, line, reply, cap, rc, ctl_fwd, c);
}
/* I676b/I676c: ARexx THEME [path]. The primary UI checks the file and repaints itself live (SEL_UI_THEME); no reopen. */
static int arexx_theme(void* ctx, const char* path) {
    shell_ui* u = &g_uis[0];
    (void)ctx;
    return u->theme == NULL || u->state == NULL || u->theme(u->state, path) != 0;
}
/* Every way out of the shell loop closes the port through here: an open BEGIN is ended, queued messages get RC 20. */
static void arexx_shutdown(arexx_port* p, ctl_ctx* c) {
    if (p != NULL) {
        arexx_close(p, arexx_exec, c, sel_engine_in_batch(c->engine));
    }
}
#endif
#ifndef SEL_NO_CONTROL_SOCKET
static int ctl_handle(void* ctx, const char* line, char* reply, size_t cap) {
    ctl_ctx* c = ctx;
    char out[SEL_CTL_REPLY_MAX - 8u]; /* room for "OK\n" and "\n." around the result lines */
    int rc;
    sel_ctl_exec_fwd(c->engine, line, out, sizeof out, &rc, ctl_fwd, c);
    if (rc != 0) {
        snprintf(reply, cap, "ERROR %s", out);
    } else if (out[0] != '\0') {
        snprintf(reply, cap, "OK\n%s\n.", out);
    } else {
        snprintf(reply, cap, "OK");
    }
    return rc == 0;
}
#endif

#ifdef __amigaos__
/* libnix polls Ctrl-C inside stdio and exit()s on it, which skips the UI close and leaves the RTG screen open (I287).
 * An empty override keeps the signal for the UI's own Wait/SetSignal, which quits through its normal close path. */
void __chkabort(void) {}

static int app_main(int argc, char** argv);

int main(int argc, char** argv) {
    return sel_amiga_run_with_stack(app_main, argc, argv, SEL_AMIGA_STACK_BYTES);
}

static int app_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    const sel_host* host = sel_host_default();
    const sel_tag* machine_tags = sel_machine_probe(host); /* probed once; the registry hides what it cannot run */
    const char* out_id;
    const char* ui_id = option(argc, argv, "--ui", "text");
    const char* start = option(argc, argv, "--start-dir", NULL);
    const char* catalogue = option(argc, argv, "--catalogue", NULL);
    sel_library* library = NULL;
    int a;
    int is_null;
    int clockless;
    const sel_tag* oh;
    sel_engine_config cfg;
    char cfgdir[256];
    sel_engine* e;
    sel_out_open_fn out_open;
    sel_out_close_fn out_close;
    sel_out_set_source_fn out_set_source;
    sel_out_pump_fn out_pump;
    sel_ui_open_args args;
    int input_consumed = 0; /* the --input script is consumed once per process, whichever UI replays it (I246) */
    sel_ui_art art_src;
    sel_engine_stats est;
    const sel_tag* ust;
    void* out;
    shell_output so;
    uint32_t rate;
    void* ctx[3];
    unsigned u;
    char console_err[160] = "";
    const char* fallback_why = NULL; /* set when the text UI replaced a MUI that could not open (I671b) */
    int has_stats = 0;
#if !defined(__amigaos__) && !defined(AMIGA)
    char cwd[1024];
#endif
    char cmd[1100];
    uint32_t waits = 0;
    uint32_t wait_ms_min = 0xFFFFFFFFu; /* the smallest and largest UI wait the loop asked for (I684c) */
    uint32_t wait_ms_max = 0;
    int r;
    int has_keys = option(argc, argv, "--keys", NULL) != NULL;
    /* Preferred media-session integration per OS; sel_registry_default_integration falls back
     * to "none" when it is not built (Amiga, Linux without libsystemd, Windows). */
#if defined(__APPLE__)
    const char* prefer_int = "nowplaying";
#elif defined(__linux__)
    const char* prefer_int = "mpris";
#else
    const char* prefer_int = "";
#endif
    const char* int_id;
    sel_int_session* isess = NULL;
    int ctl_fd = -1;
    unsigned socket_cmds = 0;
    (void)ctl_fd;
    (void)socket_cmds;
#ifndef SEL_NO_CONTROL_SOCKET
    char ctl_path[512];
#endif
#if !defined(SEL_NO_CONTROL_SOCKET) || defined(__amigaos__)
    ctl_ctx cc;
#endif
#ifdef __amigaos__
    arexx_port* arexx = NULL;
#endif

    {
        /* --log <file>: stderr (all startup errors) goes to the file; the Amiga shell cannot redirect libnix stderr. */
        const char* log_path = option(argc, argv, "--log", "");
        if (log_path[0] != '\0' && freopen(log_path, "w", stderr) == NULL) {
            return 1;
        }
    }
    {
        int ta;
        for (ta = 1; ta < argc; ta++) {
            g_trace = g_trace || strcmp(argv[ta], "--stats") == 0;
        }
    }
    sel_trace(host, "main begin");
    sel_registry_init_machine(host, machine_tags);
    sel_trace(host, "registry init done");
    out_id = option(argc, argv, "--output", sel_registry_default_output(machine_tags));
    is_null = strcmp(out_id, "null") == 0;
    int_id = option(argc, argv, "--integration",
                    (is_null || has_keys) ? "none" : sel_registry_default_integration(prefer_int));
    oh = hooks_of(SEL_PLUGIN_KIND_OUTPUT, out_id);
    for (a = 1; a < argc; a++) {
        if (strcmp(argv[a], "--ui") == 0 && a + 1 < argc) {
            if (g_ui_count >= SHELL_MAX_UIS) {
                STARTUP_FAIL("too many --ui");
                return 2;
            }
            if (!ui_resolve(argv[a + 1], &g_uis[g_ui_count])) {
                STARTUP_FAIL("ui '%s' is not registered", argv[a + 1]);
                return 2;
            }
            g_ui_count++;
        }
        if (strcmp(argv[a], "--stats") == 0) {
            has_stats = 1;
            g_stats = 1;
        }
    }
    if (g_ui_count == 0 && !ui_resolve(ui_id, &g_uis[g_ui_count++])) {
        STARTUP_FAIL("ui '%s' is not registered", ui_id);
        return 2;
    }
    if (oh == NULL) {
        STARTUP_FAIL("output '%s' is not registered", out_id);
        return 2;
    }
    out_open = (sel_out_open_fn)sel_tag_find(oh, SEL_OUT_OPEN, 0);
    out_close = (sel_out_close_fn)sel_tag_find(oh, SEL_OUT_CLOSE, 0);
    out_set_source = (sel_out_set_source_fn)sel_tag_find(oh, SEL_OUT_SET_SOURCE, 0);
    out_pump = (sel_out_pump_fn)sel_tag_find(oh, SEL_OUT_PUMP, 0);
    clockless = sel_tag_find(oh, SEL_OUT_CLOCKLESS, 0) != 0;
    ctx[0] = NULL;
    if (!out_open || !out_close || !out_set_source || !out_pump) {
        STARTUP_FAIL("a plugin lacks a hook");
        return 2;
    }

    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = machine_tags; /* queue and browse buffers scale with the machine */
    if (catalogue != NULL) {
        library = sel_cli_open_catalogue(catalogue);
        if (library == NULL) {
            return 1;
        }
        cfg.library = library;
    } else {
        /* The persistent library (I205): --db, else the default path. A scripted or null-output run
         * never touches the user's default DB, so a test does not depend on the machine it runs on. */
        const char* db = option(argc, argv, "--db", NULL);
        char dbpath[300];
        if (db == NULL && !has_keys && !is_null) {
            db = default_db_path(dbpath, sizeof dbpath);
        }
        if (db != NULL) {
            library = sel_library_open(host, machine_tags, 0, db);
            if (library == NULL) {
                STARTUP_FAIL("cannot open the library database %s", db);
                return 1;
            }
            cfg.library = library;
        }
    }
    cfg.config_dir = option(argc, argv, "--config-dir", NULL);
    if (cfg.config_dir == NULL && sel_config_dir(cfgdir, sizeof cfgdir)) {
        cfg.config_dir = cfgdir;
    }
    sel_trace(host, "library open done");
    e = sel_engine_create(host, &cfg);
    sel_trace(host, "engine created");
    if (e == NULL) {
        STARTUP_FAIL("engine allocation failed");
        return 1;
    }
    rate = sel_registry_output_rate(oh, machine_tags);
    sel_engine_set_mix(e, rate, SHELL_RESAMPLER);
    sel_engine_set_output(e, oh);
    out = out_open(host, rate, 2);
    if (out == NULL) {
        STARTUP_FAIL("no output device");
        sel_engine_destroy(e);
        return 77;
    }
    g_mark_audio = !clockless;
    out_set_source(out, audio_pull, e);
    so.rate = rate;
    so.open = out_open;
    so.close = out_close;
    so.set_source = out_set_source;
    so.pump = out_pump;
    so.machine = machine_tags;
    so.host = host;
    so.engine = e;
    so.out = out;
    so.paced = sel_tag_find(oh, SEL_OUT_UNPACED, 0) == 0;
    sel_engine_set_handover(e, shell_handover, &so);
    sel_engine_set_output_switch(e, shell_switch_output, &so);

    memset(&args, 0, sizeof args);
    args.engine = e;
    args.host = host;
    args.input_consumed = &input_consumed;
    args.find_plugin = sel_registry_find;
    args.argc = argc;
    args.argv = (const char* const*)argv;
    if (library != NULL) {
        sel_library_art_provider(library, &art_src);
        args.art = &art_src;
    }
    sel_trace(host, "output open done, UI open begin");
    for (u = 0; u < g_ui_count; u++) {
        g_uis[u].state = g_uis[u].open(&args);
        sel_trace(host, "UI open returned");
        /* Only the MUI plugin that cannot open (MUI-P1-4, Q26) has already said why (requester); the shell keeps the text UI.
         * Every other UI that refuses (bad --format, --clock, scale, no SDL) must fail the start with rc 1. */
        if (g_uis[u].state == NULL && strcmp(g_uis[u].id, "mui") == 0 && ui_resolve("text", &g_uis[u])) {
            /* The text UI needs a console; Run >NIL: and a Workbench start have none (I671b), so open one first. */
            if (sel_console_ensure(console_err, sizeof console_err)) {
                g_uis[u].state = g_uis[u].open(&args);
                if (g_uis[u].state == NULL) {
                    snprintf(console_err, sizeof console_err, "the text UI could not open either");
                }
            }
            fallback_why = console_err;
        }
        if (g_uis[u].state == NULL) {
            if (fallback_why != NULL) {
                STARTUP_FAIL("the MUI window could not open, and the text UI fallback failed: %s", fallback_why);
            } else {
                STARTUP_FAIL("the UI could not open (no terminal?)");
            }
            while (u-- > 0) {
                g_uis[u].close(g_uis[u].state);
            }
            if (so.out != NULL) {
                so.close(so.out);
            }
            sel_engine_destroy(e);
            return 1;
        }
    }
    ctx[0] = e;
    sel_engine_set_ui_view_styles(e, g_uis[0].view_styles);
    ctx[1] = g_uis[0].state;
    ctx[2] = NULL;

#if !defined(SEL_NO_CONTROL_SOCKET) || defined(__amigaos__)
    cc.engine = e;
    cc.sink_ctx = ctx;
#endif
#ifdef __amigaos__
    arexx = arexx_open(); /* after the engine exists, before the first UI turn; NULL leaves ARexx off */
    arexx_set_theme_handler(arexx_theme, NULL);
#endif
#ifndef SEL_NO_CONTROL_SOCKET
    /* I573: a scripted run (--keys, or the desktop UI's --input feeder) opens no control socket unless SELECTA_SOCKET asks.
     * With the socket open, a clockless run polls it SHELL_TURN_MS per turn, so a headless --input run plays its song in
     * real time, and whether the socket binds depends on the TMPDIR length (sun_path), which made run time path-dependent. */
    if (!(has_keys || option(argc, argv, "--input", NULL) != NULL) || getenv("SELECTA_SOCKET") != NULL) {
        sel_ctl_path(ctl_path, sizeof ctl_path);
        ctl_fd = sel_ctl_listen(ctl_path);
        if (ctl_fd == SEL_CTL_ALREADY_RUNNING) {
            STARTUP_FAIL("already running (%s)", ctl_path);
            for (u = 0; u < g_ui_count; u++) {
                g_uis[u].close(g_uis[u].state);
            }
            if (so.out != NULL) {
                so.close(so.out);
            }
            sel_engine_destroy(e);
            return 1;
        }
    }
#endif
    if (strcmp(int_id, "none") != 0) {
        isess = sel_int_session_open(e, int_id, sink, ctx);
        if (isess == NULL) {
            STARTUP_FAIL("integration '%s' is not available", int_id);
        }
        ctx[2] = isess;
    }

    if (start == NULL) {
#if defined(__amigaos__) || defined(AMIGA)
        start = "PROGDIR:";
#else
        start = getcwd(cwd, sizeof cwd);
#endif
    }
    if (start != NULL && sel_path_is_absolute(start)) {
        snprintf(cmd, sizeof cmd, "LIBRARY_BROWSE %s", start);
        send_line(e, cmd);
        const char* view = option(argc, argv, "--view", "");
        int own = strcmp(ui_id, "desktop") == 0 && (view[0] == '\0' || strcmp(view, "home") == 0 || strcmp(view, "library") == 0 || strcmp(view, "folders") == 0 || /* no --view: the desktop UI starts on Home (I594) */
                                                     strcmp(view, "playlists") == 0 || strcmp(view, "songs") == 0);
        /* the desktop UI enters these pages itself (desk_page_goto), the one place their engine verbs live (I266) */
        if (!own) {
            if (strcmp(option(argc, argv, "--view", ""), "home") == 0) { /* the start view the user named: Home, not the folder list */
                send_line(e, "VIEW_SOURCE home");
                send_line(e, "VIEW_WINDOW 32");
                sel_engine_pump(e, sink, ctx);
            } else if (strcmp(option(argc, argv, "--view", ""), "library") == 0) { /* the Library page: composers and the stats (DUI-P4-3) */
                send_line(e, "VIEW_SHOW browse");
                send_line(e, "COMPOSERS_SHOW");
                send_line(e, "STATS_SHOW");
            } else if (strcmp(option(argc, argv, "--view", ""), "playlists") == 0 || strcmp(option(argc, argv, "--view", ""), "songs") == 0) {
                send_line(e, "VIEW_SOURCE folders"); /* the saved playlists: the Playlists page lists them, the Songs page drops onto them */
                send_line(e, "VIEW_WINDOW 64");
            } else {
                send_line(e, "VIEW_SHOW browse");
            }
        }
    }
    apply_option_words(e, argc, argv);
    sel_engine_pump(e, sink, ctx);
    for (a = 1; a < argc; a++) { /* `selecta <share link>` opens the song, playlist or view the link names (I180) */
        if (sel_share_is_link(argv[a])) {
            char pool[1536];
            snprintf(cmd, sizeof cmd, "OPEN %s", argv[a]);
            if (sel_engine_send_line(e, cmd, pool, sizeof pool) == SEL_CODEC_OK) {
                sel_engine_pump(e, sink, ctx);
            }
        }
    }
    for (a = 1; a < argc; a++) { /* the song named on the command line is opened and played, whatever the UI (I674) */
        if (is_song_argument(argc, argv, a)) {
            char pool[1536];
            snprintf(cmd, sizeof cmd, "OPEN %s", argv[a]);
            if (sel_engine_send_line(e, cmd, pool, sizeof pool) == SEL_CODEC_OK) {
                sel_engine_pump(e, sink, ctx);
                send_line(e, "PLAY");
            }
            break;
        }
    }
    for (a = 1; a + 1 < argc; a++) {
        if (strcmp(argv[a], "--exec") == 0) {
            char pool[1024];
            if (strncmp(argv[a + 1], "UI_SWITCH ", 10) == 0) { /* the shell owns the UIs, not the engine */
                if (ui_switch(argv[a + 1] + 10, &args, ctx) < 0) {
                    #ifdef __amigaos__
                    arexx_shutdown(arexx, &cc);
                    #endif
                    return 1;
                }
            } else if (sel_engine_send_line(e, argv[a + 1], pool, sizeof pool) == SEL_CODEC_OK) {
                sel_engine_pump(e, sink, ctx);
                sel_engine_scan_wait(e, sink, ctx); /* a --exec LIBRARY_SCAN finishes before the next option (I336) */
            }
        }
    }

    /* A clockless output (null, wav): each turn pumps exactly one UI_WAIT's worth
     * of frames, so a run is deterministic. A real output paces the loop. */
    for (;;) {
        r = -1;
        for (u = 0; u < g_ui_count; u++) { /* the loop runs while any UI still has a turn to take */
            uint32_t wait_ms = sel_shell_wait_ms(clockless, so.out != NULL, so.paced, SHELL_TURN_MS);
            int wr;
            if (wait_ms < wait_ms_min) {
                wait_ms_min = wait_ms;
            }
            if (wait_ms > wait_ms_max) {
                wait_ms_max = wait_ms;
            }
            wr = g_uis[u].wait(g_uis[u].state, wait_ms);
            if (wr > r) {
                r = wr;
            }
        }
        sel_engine_pump(e, sink, ctx); /* a UI_SWITCH key sent during the wait is waiting in the engine */
        {
            char want[16];
            if (sel_engine_take_ui_switch(e, want, sizeof want)) { /* the shell owns the UIs: switch between turns */
                if (ui_switch(want, &args, ctx) < 0) {
                    #ifdef __amigaos__
                    arexx_shutdown(arexx, &cc);
                    #endif
                    return 1;
                }
                continue;
            }
        }
        if (r < 0 || sel_engine_take_quit(e)) { /* QUIT ends the run like the primary UI's quit key */
            break;
        }
        waits++;
        if (so.out != NULL) {
            g_pumping = 1;
            so.pump(so.out, so.rate * SHELL_TURN_MS / 1000u);
            g_pumping = 0;
        }
        sel_engine_pump(e, sink, ctx);
        sel_int_session_poll(isess);
#ifndef SEL_NO_CONTROL_SOCKET
        if (ctl_fd >= 0) {
            socket_cmds += (unsigned)sel_ctl_serve(ctl_fd, sel_shell_wait_ms(clockless, so.out != NULL, so.paced, SHELL_TURN_MS), ctl_handle, &cc);
        }
#endif
#ifdef __amigaos__
        if (arexx != NULL) { /* the Amiga's control path, polled where the socket is polled (M4-AX-3-2) */
            arexx_poll(arexx, arexx_exec, &cc);
        }
#endif
    }

    ust = ui_stats_of(&g_uis[0]);
    sel_engine_stats_get(e, &est);
    printf("shell_wait_ms_min=%u shell_wait_ms_max=%u\n", (unsigned)(wait_ms_min == 0xFFFFFFFFu ? 0u : wait_ms_min), (unsigned)wait_ms_max); /* what the shell asked the UI to wait (I684c) */
    printf("ui_waits=%u flushes=%u keys=%u\n", (unsigned)waits, (unsigned)sel_tag_find(ust, SEL_UI_STAT_FLUSHES, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_KEYS, 0));
    printf("hw_handovers=%u hw_starts=%u replayer=%s\n", (unsigned)est.hw_handovers, (unsigned)est.hw_starts,
           est.replayer != NULL ? est.replayer : "none");
    printf("songs_opened=%u queue_version=%u\n", (unsigned)est.songs_opened,
           (unsigned)sel_engine_state(e)->queue_version);
    if (has_stats) { /* the song the engine has open, for the mouse test (DUI-P3-2) */
        printf("song=%s\n", sel_engine_state(e)->song.path != NULL ? sel_engine_state(e)->song.path : "");
    }
    printf("links_parsed=%u links_made=%u\n", (unsigned)est.links_parsed, (unsigned)est.links_made);
    printf("sources_listed=%u evt_dropped=%u\n", (unsigned)est.sources_listed, (unsigned)est.evt_dropped);

    printf("cmdline_runs=%u palette_opens=%u bindings=%u playlist_entries=%u undo_depth=%u redo_depth=%u view_style=%s\n",
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_CMDLINE_RUNS, 0), (unsigned)sel_tag_find(ust, SEL_UI_STAT_PALETTE_OPENS, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_BINDINGS, 0), (unsigned)sel_engine_state(e)->queue_count,
           (unsigned)est.undo_depth, (unsigned)est.redo_depth, sel_view_style_label(sel_engine_state(e)->view->style));

    printf("seek_path=%u volume=%d view_cursor=%u\n", (unsigned)est.seek_path, (int)sel_engine_state(e)->volume,
           (unsigned)sel_engine_state(e)->view->cursor);
    printf("view_window=%u\n", (unsigned)sel_engine_state(e)->view->window_rows);
    printf("view_source=%s view_style=%s cursor_key=%s\n", sel_view_source_label(sel_engine_state(e)->view->source),
           sel_view_style_label(sel_engine_state(e)->view->style),
           sel_engine_view_cursor_key(e) != NULL ? sel_engine_view_cursor_key(e) : "");
    printf("view_rows=%u\n", (unsigned)sel_engine_state(e)->view->row_count); /* I473: how many rows the view lists */

    printf("viz_frames=%u scope_reads=%u pattern_rows=%u mutes=%u dsp=crossfeed:%s fades=%u sleep_armed=%u\n",
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_VIZ_FRAMES, 0), (unsigned)sel_tag_find(ust, SEL_UI_STAT_SCOPE_READS, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_PATTERN_ROWS, 0), (unsigned)est.mutes,
           est.crossfeed_on ? "on" : "off", (unsigned)est.fades, (unsigned)est.sleep_armed);

    printf("ui_frames=%u ui_hash=%08x\n", (unsigned)sel_tag_find(ust, SEL_UI_STAT_FRAMES, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_HASH, 0));

    if (has_stats) { /* one line per open UI (DUI-P1-4) */
        for (u = 0; u < g_ui_count; u++) {
            const sel_tag* us = ui_stats_of(&g_uis[u]);
            printf("ui_open_mui=%u\n", (unsigned)sel_tag_find(us, SEL_UI_STAT_OPEN_MUI, 0)); /* MUI-P1-5 sentinel */
            printf("ui_stats id=%s frames=%u widget_draws=%u uim_updates=%u uim_dirty=%u uim_layout=%u tweens=%u effect_prims=%u dirty_pixels=%u dirty_total=%u canvas_hash=%s draw_prims=%u missing_glyphs=%u glyphs=%u view_opens=%u art_decodes=%u art_tile_hash=%s present_hash=%s upload_pixels=%u presents=%u scale=%u atlas_rebuilds=%u icon_rebuilds=%u atlas_builds=%u actions_fired=%u actions_distinct=%u focus_moves=%u searches=%u arrows=%u focus=%u hovers=%u clicks=%u double_clicks=%u drags=%u sorts=%u sort_dir=%u top_row=%u menus=%u row_plays=%u tooltips=%u gestures=%u tile_size=%u scroll_px=%u open_tile=%u transport_hits=%u play_state=%u repeat_mode=%u random_on=%u subsong=%u frame_parts=%u home_parts=%u tile_cards=%u palette_colours_ui=%u palette_colours_art=%u palette_colours_tile=%u palette_colours_panel=%u palette_colours_flow=%u quant_calls=%u art_ms_max=%u turn_ms_max=%u frame_ms_min=%u frame_ms_avg=%u frame_ms_max=%u page_dirty_pixels=%u paint_ms_min=%u paint_ms_avg=%u paint_ms_max=%u present_ms_min=%u present_ms_avg=%u present_ms_max=%u atlas_ms=%u atlas_baked_hits=%u icon_resampled=%u ui_scale_q16=%u ui_zoom=%s ui_logical=%ux%u shelf_album_art_draws=%u tooltip_targets=%u bitmap_hits_mini4=%u bitmap_hits_mini6=%u bitmap_hits_mini8=%u headline_hits_18=%u headline_hits_24=%u\n", g_uis[u].id,
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_FRAMES, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_WIDGET_DRAWS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_UIM_UPDATES, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_UIM_DIRTY, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_UIM_LAYOUT, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_TWEENS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_EFFECT_PRIMS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_DIRTY_PIXELS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_DIRTY_TOTAL, 0),
                   stat_hash_str(us, SEL_UI_STAT_CANVAS_HASH),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_DRAW_PRIMS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_MISSING_GLYPHS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_GLYPHS_DRAWN, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_VIEW_OPENS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ART_DECODES, 0),
                   stat_hash_str(us, SEL_UI_STAT_ART_TILE_HASH),
                   stat_hash_str(us, SEL_UI_STAT_PRESENT_HASH),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_UPLOAD_PIXELS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PRESENTS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SCALE, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_ATLAS_REBUILDS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ICON_REBUILDS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_ATLAS_BUILDS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ACTIONS_FIRED, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ACTIONS_DISTINCT, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_FOCUS_MOVES, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_SEARCHES, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ARROWS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_FOCUS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_HOVERS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_CLICKS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_DOUBLE_CLICKS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_DRAGS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SORTS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_SORT_DIR, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_TOP_ROW, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_MENUS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ROW_PLAYS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_TOOLTIPS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_GESTURES, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_TILE_SIZE, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SCROLL_PX, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_OPEN_TILE, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_TRANSPORT_HITS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_PLAY_STATE, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_REPEAT_MODE, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_RANDOM_ON, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SUBSONG, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_FRAME_PARTS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_HOME_PARTS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_TILE_CARDS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PALETTE_COLOURS_UI, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PALETTE_COLOURS_ART, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PALETTE_COLOURS_TILE, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PALETTE_COLOURS_PANEL, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PALETTE_COLOURS_FLOW, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_QUANT_CALLS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_ART_MS_MAX, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_TURN_MS_MAX, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_FRAME_MS_MIN, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_FRAME_MS_AVG, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_FRAME_MS_MAX, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_PAGE_DIRTY, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PAINT_MS_MIN, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_PAINT_MS_AVG, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PAINT_MS_MAX, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_PRESENT_MS_MIN, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_PRESENT_MS_AVG, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_PRESENT_MS_MAX, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ATLAS_MS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_ATLAS_BAKED_HITS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_ICON_RESAMPLED, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_SCALE_Q16, 65536u), (const char*)sel_tag_find(us, SEL_UI_STAT_ZOOM, (uintptr_t) "100"),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_LOGICAL_W, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_LOGICAL_H, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SHELF_ALBUM_ART_DRAWS, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_TOOLTIP_TARGETS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_BITMAP_HITS_MINI4, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_BITMAP_HITS_MINI6, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_BITMAP_HITS_MINI8, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_HEADLINE_HITS_18, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_HEADLINE_HITS_24, 0));
            {
                static const char* const pcls[] = {"fill", "scrim", "round", "gradient", "line", "text", "mask", "image", "shadow", "icon", "genart", "dirty", "clip", "palette", "quant", "art"};
                static const char* const ppt[] = {"canvas", "song", "sidebar", "main", "transport", "progress", "volume", "status", "focus", "tip", "overlay", "visual"};
                size_t k;
                printf("ui_paint id=%s span_px=%u readbacks=%u", g_uis[u].id, (unsigned)sel_tag_find(us, SEL_UI_STAT_SPAN_PIXELS, 0),
                       (unsigned)sel_tag_find(us, SEL_UI_STAT_READBACKS, 0));
                for (k = 0; k < sizeof pcls / sizeof pcls[0]; k++) {
                    printf(" paint_ms_%s=%u paint_n_%s=%u", pcls[k], (unsigned)sel_tag_find(us, SEL_UI_STAT_PROF_MS_FILL + (uint32_t)k, 0),
                           pcls[k], (unsigned)sel_tag_find(us, SEL_UI_STAT_PROF_N_FILL + (uint32_t)k, 0));
                }
                for (k = 0; k < sizeof ppt / sizeof ppt[0]; k++) {
                    printf(" painter_ms_%s=%u", ppt[k], (unsigned)sel_tag_find(us, SEL_UI_STAT_PT_MS_CANVAS + (uint32_t)k, 0));
                }
                printf("\n");
                printf("ui_flow id=%s", g_uis[u].id);
                printf(" rest_n=%u rest_paint_ms=%u rest_max_ms=%u slide_n=%u slide_paint_ms=%u slide_max_ms=%u",
                       (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_N_REST, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_PAINT_REST, 0),
                       (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_MAX_REST, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_N_SLIDE, 0),
                       (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_PAINT_SLIDE, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_MAX_SLIDE, 0));
                for (k = 0; k < sizeof pcls / sizeof pcls[0]; k++) {
                    printf(" rest_%s=%u slide_%s=%u", pcls[k], (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_MS_REST + (uint32_t)k, 0), pcls[k],
                           (unsigned)sel_tag_find(us, SEL_UI_STAT_FLOW_MS_SLIDE + (uint32_t)k, 0));
                }
                printf("\n");
            }
            printf("ui_library id=%s\n%s", g_uis[u].id, (const char*)sel_tag_find(us, SEL_UI_STAT_LIBRARY_DUMP, (uintptr_t) ""));
            printf("ui_drop id=%s drops=%u scans=%u\n", g_uis[u].id, (unsigned)sel_tag_find(us, SEL_UI_STAT_DROPS, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SCANS, 0));
            printf("ui_list id=%s %s\n", g_uis[u].id, (const char*)sel_tag_find(us, SEL_UI_STAT_LIST_VIEW, (uintptr_t) ""));
            printf("ui_p4 id=%s %s\n", g_uis[u].id, (const char*)sel_tag_find(us, SEL_UI_STAT_MUI_P4, (uintptr_t) ""));
            printf("ui_backend id=%s backend=%s depth=%u\n", g_uis[u].id,
                   (const char*)sel_tag_find(us, SEL_UI_STAT_BACKEND, (uintptr_t)""),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_SCREEN_DEPTH, 0));
            printf("ui_tooltip id=%s text=%s\n", g_uis[u].id, (const char*)sel_tag_find(us, SEL_UI_STAT_TOOLTIP_TEXT, (uintptr_t) ""));
            printf("ui_home id=%s shelves=%u shelf_scroll=%u scroll_y=%u tile_queued=%u empty=%u\n%s", g_uis[u].id,
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_HOME_SHELVES, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_SHELF_SCROLL, 0),
                   (unsigned)sel_tag_find(us, SEL_UI_STAT_HOME_SCROLL_Y, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_TILE_QUEUED, 0), (unsigned)sel_tag_find(us, SEL_UI_STAT_HOME_EMPTY, 0), (const char*)sel_tag_find(us, SEL_UI_STAT_HOME_DUMP, (uintptr_t) ""));
        }
    }

    printf("viz_marks dot_rows=%u phase_points=%u\n", (unsigned)sel_tag_find(ust, SEL_UI_STAT_DOT_ROWS, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_PHASE_POINTS, 0));

    printf("library_screen opens=%u searches=%u\n", (unsigned)sel_tag_find(ust, SEL_UI_STAT_LIBRARY_OPENS, 0),
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_LIBRARY_SEARCHES, 0));
    printf("folders_screen opens=%u rows_built=%u\n", (unsigned)sel_tag_find(ust, SEL_UI_STAT_FOLDER_OPENS, 0),
           (unsigned)est.folder_view_rows);
    printf("home_view home_rows=%u home_item_shelves=%u\n", (unsigned)est.home_rows, (unsigned)est.home_item_shelves);
    printf("stats_screen stats_queries=%u\n", (unsigned)est.stats_queries);
    printf("splash_frames=%u\n", (unsigned)sel_tag_find(ust, SEL_UI_STAT_SPLASH_FRAMES, 0));
    printf("meta_panel panel_loads=%u meta_panel_rows=%u\n", (unsigned)est.panel_loads,
           (unsigned)sel_tag_find(ust, SEL_UI_STAT_META_PANEL_ROWS, 0));

    printf("engine_state pitch_cents=%d tempo_percent=%d mute_mask=%u mutes=%u\n", (int)sel_engine_state(e)->pitch_cents,
           (int)sel_engine_state(e)->tempo_percent, (unsigned)sel_engine_state(e)->mute_mask, (unsigned)est.mutes);

#ifdef __amigaos__
    {
        char line[128];
        static const arexx_counters none = {0, 0};
        arexx_stats_line(line, sizeof line, arexx != NULL ? arexx_name(arexx) : NULL,
                         arexx != NULL ? arexx_stats(arexx) : &none);
        printf("%s\n", line);
        arexx_shutdown(arexx, &cc);
    }
#endif
#ifndef SEL_NO_CONTROL_SOCKET
    if (ctl_fd >= 0) {
        printf("socket_cmds=%u\n", socket_cmds);
        sel_ctl_close(ctl_fd, ctl_path);
    }
#endif
    sel_int_session_close(isess);
    for (u = 0; u < g_ui_count; u++) {
        g_uis[u].close(g_uis[u].state);
    }
    if (so.out != NULL) {
        so.close(so.out);
    }
    sel_engine_destroy(e);
    if (library != NULL) {
        sel_library_cache_report(library); /* cache_hits= fetches= ... after LIBRARY_FETCH (I166a) */
        sel_library_close(library);
    }
    return 0;
}
