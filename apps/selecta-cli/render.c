/* render <song> --output <wav> --seconds <n> --rate <hz> --resample <name>
 * (T13, T17): opens a song in the engine, renders it through the wav
 * output and prints `ticks=N voice_ops=V mixed_frames=F`. The wav output
 * takes its path from the host config key "wav.path", so the command
 * builds a copy of the default host that answers it. SEL_ALLOC_FREEZE and
 * SEL_ALLOC_THAW on stderr bracket the render loop (no-alloc test). */
#include <selecta/engine.h>
#include <selecta/output.h>
#include <selecta/registry.h>

#include "cli.h"
#include "mixer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct render_ctx {
    const char* wav_path;
    int failed;
} render_ctx;

static const char* render_config(const sel_host* host, const char* key, const char* def) {
    const render_ctx* c = (const render_ctx*)host->ctx;
    if (strcmp(key, "wav.path") == 0) {
        return c->wav_path;
    }
    return def;
}

static void render_sink(void* ctx, const sel_message* evt) {
    render_ctx* c = (render_ctx*)ctx;
    if (evt->id == SEL_EVT_ERROR) {
        fprintf(stderr, "render: error %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
        c->failed = 1;
    } else if (evt->id == SEL_EVT_SEEKED) {
        printf("SEEKED %u\n", (unsigned)evt->args[0].v.u);
    }
}

static int parse_resampler(const char* name, uint32_t* out) {
    static const char* const names[] = {"none", "linear", "cubic", "sinc"};
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        if (strcmp(name, names[i]) == 0) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

int sel_cli_render(int argc, char** argv) {
    sel_host host = *sel_host_default();
    render_ctx ctx;
    sel_engine_stats stats;
    sel_cli_output o;
    const char* song = NULL;
    const char* wav = NULL;
    const char* seek_ms = NULL;
    unsigned long rate = 44100, seconds = 10;
    uint32_t resampler = SEL_MIX_RESAMPLE_LINEAR;
    sel_engine* e;
    void* out;
    uint32_t total, pumped = 0;
    int i;

    if (argc < 1) {
        fputs("render: song path missing\n", stderr);
        return 2;
    }
    song = argv[0];
    for (i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--output") == 0) {
            wav = argv[i + 1];
        } else if (strcmp(argv[i], "--seconds") == 0) {
            seconds = strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--seek-ms") == 0) {
            seek_ms = argv[i + 1];
        } else if (strcmp(argv[i], "--rate") == 0) {
            rate = strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--resample") == 0) {
            if (!parse_resampler(argv[i + 1], &resampler)) {
                fprintf(stderr, "render: unknown resampler %s\n", argv[i + 1]);
                return 2;
            }
        } else {
            fprintf(stderr, "render: unknown option %s\n", argv[i]);
            return 2;
        }
    }
    if (wav == NULL || rate == 0) {
        fputs("render: --output <wav> and a nonzero --rate are required\n", stderr);
        return 2;
    }

    ctx.wav_path = wav;
    ctx.failed = 0;
    host.config_get = render_config;
    host.ctx = &ctx;

    sel_registry_init(&host);
    if (!sel_cli_find_output("render", "wav", &o)) {
        return 1;
    }
    e = sel_cli_start_song("render", &host, song, (uint32_t)rate, resampler);
    if (e == NULL) {
        return 1;
    }

    if (seek_ms != NULL) {
        sel_message msg;
        memset(&msg, 0, sizeof msg);
        msg.kind = SEL_MSG_CMD;
        msg.id = SEL_CMD_SEEK;
        msg.argc = 1;
        msg.args[0].type = SEL_ARG_STR;
        msg.args[0].v.s = seek_ms;
        sel_engine_send(e, &msg);
        sel_engine_pump(e, render_sink, &ctx);
        {
            const sel_rp_position* pp = &sel_engine_state(e)->song.pattern_pos;
            printf("seek_position order=%u row=%u pattern=%u\n", (unsigned)pp->order, (unsigned)pp->row,
                   (unsigned)pp->pattern);
        }
    }

    out = o.open(&host, (uint32_t)rate, 2);
    if (out == NULL) {
        sel_engine_destroy(e);
        fputs("render: output open failed\n", stderr);
        return 1;
    }
    o.set_source(out, sel_cli_engine_pull, e);

    total = (uint32_t)(seconds * rate);
    fputs("SEL_ALLOC_FREEZE\n", stderr);
    fflush(stderr);
    while (pumped < total) {
        uint32_t want = total - pumped;
        uint32_t got;
        if (want > 512u) {
            want = 512u;
        }
        got = o.pump(out, want);
        if (got == 0) {
            break;
        }
        pumped += got;
    }
    fputs("SEL_ALLOC_THAW\n", stderr);
    fflush(stderr);

    sel_engine_pump(e, render_sink, &ctx);
    sel_engine_stats_get(e, &stats);
    if (seek_ms != NULL) {
        printf("seek_path=%s skipped_frames=%u\n", stats.seek_path == 1u ? "native" : "skip",
               (unsigned)stats.seek_skipped_frames);
    }
    if (stats.render_kind) {
        printf("replayer=%s render_calls=%u rendered_frames=%u\n", stats.replayer, (unsigned)stats.replayer_render_calls,
               (unsigned)stats.rendered_frames);
    } else {
        printf("ticks=%u voice_ops=%u mixed_frames=%u\n", (unsigned)stats.ticks, (unsigned)stats.voice_ops,
               (unsigned)stats.mixed_frames);
    }
    o.close(out);
    sel_engine_destroy(e);
    return ctx.failed || pumped != total;
}
