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
    int script; /* --script: errors go to stdout and do not fail the render */
} render_ctx;

static const char* render_config(const sel_host* host, const char* key, const char* def) {
    const render_ctx* c = (const render_ctx*)host->ctx;
    if (strcmp(key, "wav.path") == 0) {
        return c->wav_path;
    }
    if (strcmp(key, "paula.log") == 0) { /* echo the paula-shadow register log (hand checks) */
        const char* v = getenv("SELECTA_PAULA_LOG");
        return v != NULL ? v : def;
    }
    return def;
}

static void render_sink(void* ctx, const sel_message* evt) {
    render_ctx* c = (render_ctx*)ctx;
    if (evt->id == SEL_EVT_ERROR && c->script) {
        printf("ERROR %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
    } else if (evt->id == SEL_EVT_ERROR) {
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

/* --script <file>: sends each command line (blank and # lines skipped) before the
 * first sample is pulled, pumping after each so its events print. */
static int run_script(sel_engine* e, render_ctx* c, const char* path) {
    char line[256];
    char pool[512];
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "render: cannot read %s\n", path);
        return 0;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        sel_engine_send_line(e, line, pool, sizeof pool);
        sel_engine_pump(e, render_sink, c);
    }
    fclose(f);
    return 1;
}

static uint32_t fnv1a(const int16_t* v, uint32_t n) {
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++) {
        h = (h ^ ((uint16_t)v[i] & 0xFFu)) * 16777619u;
        h = (h ^ ((uint16_t)v[i] >> 8)) * 16777619u;
    }
    return h;
}

/* One line per 20 ms block (T40): voice peaks and VU levels, the spectrum bands,
 * a hash of each voice's scope and of the post-mix ring, and the correlation. */
static void dump_viz_block(FILE* f, const sel_viz_frame* z) {
    uint32_t k;
    fprintf(f, "block=%u peaks=", (unsigned)z->seq);
    for (k = 0; k < z->voice_count; k++) fprintf(f, "%s%d", k ? "," : "", z->peak[k]);
    fputs(" vu=", f);
    for (k = 0; k < z->voice_count; k++) fprintf(f, "%s%d", k ? "," : "", z->vu[k]);
    fputs(" bands=", f);
    for (k = 0; k < SEL_VIZ_BANDS; k++) fprintf(f, "%s%d", k ? "," : "", z->band[k]);
    fputs(" scope=", f);
    for (k = 0; k < z->voice_count; k++) fprintf(f, "%s%08x", k ? "," : "", (unsigned)fnv1a(z->scope[k], SEL_VIZ_SCOPE_FRAMES));
    fprintf(f, " pcm=%08x corr=%d\n", (unsigned)fnv1a(z->pcm, SEL_VIZ_PCM_FRAMES * 2u), z->correlation);
}

int sel_cli_render(int argc, char** argv) {
    sel_host host = *sel_host_default();
    render_ctx ctx;
    sel_engine_stats stats;
    sel_cli_output o;
    const char* song = NULL;
    const char* wav = NULL;
    const char* out_id = NULL;
    const sel_voices* sink = NULL;
    const char* seek_ms = NULL;
    const char* rp_global = NULL;
    const char* rp_song = NULL;
    const char* viz_dump = NULL;
    const char* script = NULL;
    const char* dsp = NULL;
    const char* loud_mode = NULL;
    const char* loud_db = NULL;
    int loud_have_gain = 0;
    int32_t loud_gain_cdb = 0;
    FILE* dump = NULL;
    uint32_t viz_blocks = 0, viz_seen = 0;
    unsigned long rate = 0, seconds = 10; /* 0: the output's preferred rate */
    uint32_t resampler = SEL_MIX_RESAMPLE_LINEAR;
    uint32_t led_on = 0, width = 100, stage_flag = 0;
    sel_engine* e;
    void* out;
    uint32_t total, pumped = 0;
    int i;

    /* Options are `--name value` pairs in any order; the first other argument is the song. */
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) != 0) {
            if (song != NULL) {
                fprintf(stderr, "render: unexpected argument %s\n", argv[i]);
                return 2;
            }
            song = argv[i];
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "render: option %s needs a value\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--output") == 0) {
            /* A path (a slash or a .wav suffix) is the wav output; anything else names an output plugin. */
            size_t n = strlen(argv[i + 1]);
            if (strchr(argv[i + 1], '/') != NULL || (n > 4 && strcmp(argv[i + 1] + n - 4, ".wav") == 0)) {
                wav = argv[i + 1];
            } else {
                out_id = argv[i + 1];
            }
        } else if (strcmp(argv[i], "--seconds") == 0) {
            seconds = strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--viz-dump") == 0) {
            viz_dump = argv[i + 1];
        } else if (strcmp(argv[i], "--script") == 0) {
            script = argv[i + 1];
        } else if (strcmp(argv[i], "--dsp") == 0) {
            dsp = argv[i + 1];
        } else if (strcmp(argv[i], "--seek-ms") == 0) {
            seek_ms = argv[i + 1];
        } else if (strcmp(argv[i], "--rp-config") == 0) {
            rp_global = argv[i + 1];
        } else if (strcmp(argv[i], "--rp-config-song") == 0) {
            rp_song = argv[i + 1];
        } else if (strcmp(argv[i], "--rate") == 0) {
            rate = strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--led") == 0) {
            if (strcmp(argv[i + 1], "on") != 0 && strcmp(argv[i + 1], "off") != 0) {
                fputs("render: --led takes on or off\n", stderr);
                return 2;
            }
            led_on = strcmp(argv[i + 1], "on") == 0;
            stage_flag = 1;
        } else if (strcmp(argv[i], "--stereo-width") == 0) {
            width = (uint32_t)strtoul(argv[i + 1], NULL, 10);
            if (width > 100u) {
                fputs("render: --stereo-width takes 0 to 100\n", stderr);
                return 2;
            }
            stage_flag = 1;
        } else if (strcmp(argv[i], "--loudness") == 0) {
            if (strcmp(argv[i + 1], "track") != 0 && strcmp(argv[i + 1], "auto") != 0 && strcmp(argv[i + 1], "off") != 0) {
                fputs("render: --loudness takes track, auto or off\n", stderr);
                return 2;
            }
            loud_mode = argv[i + 1];
        } else if (strcmp(argv[i], "--db") == 0) {
            loud_db = argv[i + 1];
        } else if (strcmp(argv[i], "--gain-cdb") == 0) {
            loud_gain_cdb = (int32_t)strtol(argv[i + 1], NULL, 10);
            loud_have_gain = 1;
        } else if (strcmp(argv[i], "--resample") == 0) {
            if (!parse_resampler(argv[i + 1], &resampler)) {
                fprintf(stderr, "render: unknown resampler %s\n", argv[i + 1]);
                return 2;
            }
        } else {
            fprintf(stderr, "render: unknown option %s\n", argv[i]);
            return 2;
        }
        i++;
    }
    if (song == NULL) {
        fputs("render: song path missing\n", stderr);
        return 2;
    }
    if (wav == NULL && out_id == NULL) {
        fputs("render: --output <wav or output id> is required\n", stderr);
        return 2;
    }

    ctx.wav_path = wav != NULL ? wav : "";
    ctx.failed = 0;
    ctx.script = script != NULL;
    host.config_get = render_config;
    host.ctx = &ctx;

    sel_registry_init(&host);
    if (!sel_cli_find_output("render", out_id != NULL ? out_id : "wav", &o)) {
        return 1;
    }
    if (rate == 0) {
        rate = sel_registry_output_rate(o.hooks, sel_machine_probe(&host));
    }
    out = NULL;
    if (out_id != NULL) {
        /* An output with a voice backend (paula-shadow) opens first: the replayer needs its sink at start. */
        sel_out_voice_backend_fn backend = (sel_out_voice_backend_fn)sel_tag_find(o.hooks, SEL_OUT_VOICE_BACKEND, 0);
        out = o.open(&host, (uint32_t)rate, 2);
        if (out == NULL) {
            fputs("render: output open failed\n", stderr);
            return 1;
        }
        if (backend != NULL) {
            sink = backend(out);
        }
    }
    sel_cli_set_rp_config(rp_global, rp_song);
    sel_cli_set_loudness(loud_mode, loud_db, loud_have_gain, loud_gain_cdb);
    e = sel_cli_start_song_sink("render", &host, song, (uint32_t)rate, resampler, sink);
    if (e == NULL) {
        if (out != NULL) {
            o.close(out);
        }
        return 1;
    }

    sel_engine_set_output(e, o.hooks);
    sel_engine_set_stage(e, led_on, width);
    if (dsp != NULL) {
        sel_message msg;
        memset(&msg, 0, sizeof msg);
        msg.kind = SEL_MSG_CMD;
        msg.id = SEL_CMD_DSP;
        msg.argc = 2;
        msg.args[0].type = SEL_ARG_STR;
        msg.args[0].v.s = dsp;
        msg.args[1].type = SEL_ARG_STR;
        msg.args[1].v.s = "on";
        sel_engine_send(e, &msg);
        sel_engine_pump(e, render_sink, &ctx);
    }

    if (script != NULL && !run_script(e, &ctx, script)) {
        sel_engine_destroy(e);
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

    if (out == NULL) {
        out = o.open(&host, (uint32_t)rate, 2);
    }
    if (out == NULL) {
        sel_engine_destroy(e);
        fputs("render: output open failed\n", stderr);
        return 1;
    }
    o.set_source(out, sel_cli_engine_pull, e);

    if (viz_dump != NULL) {
        dump = fopen(viz_dump, "w");
        if (dump == NULL) {
            o.close(out);
            sel_engine_destroy(e);
            fprintf(stderr, "render: cannot write %s\n", viz_dump);
            return 1;
        }
    }
    total = (uint32_t)(seconds * rate);
    fputs("SEL_ALLOC_FREEZE\n", stderr);
    fflush(stderr);
    while (pumped < total) {
        uint32_t want = total - pumped;
        uint32_t got;
        /* Dump mode pulls exactly one visualisation block per call, so the
         * voice taps and the block edge line up. */
        uint32_t chunk = dump != NULL ? (uint32_t)(rate / SEL_VIZ_BLOCKS_PER_SECOND) : 512u;
        if (want > chunk) {
            want = chunk;
        }
        got = o.pump(out, want);
        if (got == 0) {
            break;
        }
        pumped += got;
        if (dump != NULL) {
            const sel_viz_frame* z;
            sel_engine_pump(e, render_sink, &ctx);
            z = sel_engine_state(e)->viz;
            if (z != NULL && z->seq != 0 && z->seq != viz_seen) {
                viz_seen = z->seq;
                viz_blocks++;
                dump_viz_block(dump, z);
            }
        }
    }
    fputs("SEL_ALLOC_THAW\n", stderr);
    fflush(stderr);

    sel_engine_pump(e, render_sink, &ctx);
    sel_engine_stats_get(e, &stats);
    if (seek_ms != NULL) {
        printf("seek_path=%s skipped_frames=%u\n", stats.seek_path == 1u ? "native" : "skip",
               (unsigned)stats.seek_skipped_frames);
    }
    if (rp_global != NULL || rp_song != NULL) {
        printf("rp_config_applied=%u\n", (unsigned)stats.rp_config_applied);
    }
    if (stage_flag || stats.led_on || stats.width_percent != 100u) {
        printf("led=%s led_frames=%u led_cutoff_hz=%u\n", stats.led_on ? "on" : "off", (unsigned)stats.led_frames,
               (unsigned)stats.led_cutoff_hz);
        printf("width=%u width_frames=%u width_mono=%u\n", (unsigned)stats.width_percent, (unsigned)stats.width_frames,
               (unsigned)stats.width_mono);
    }
    if (loud_mode != NULL || loud_db != NULL || loud_have_gain) {
        printf("loudness_source=%s gain_q16=%u limiter_hits=%u leveler_frames=%u leveler_writes=%u loudness_frames=%u\n",
               stats.loudness_source, (unsigned)stats.loudness_gain_q16, (unsigned)stats.limiter_hits,
               (unsigned)stats.leveler_frames, (unsigned)stats.leveler_writes, (unsigned)stats.loudness_frames);
    }
    if (dsp != NULL) {
        printf("dsp=%s blocks=%u\n", dsp, (unsigned)stats.dsp_blocks);
    }
    if (stats.render_kind) {
        printf("replayer=%s render_calls=%u rendered_frames=%u\n", stats.replayer, (unsigned)stats.replayer_render_calls,
               (unsigned)stats.rendered_frames);
    } else {
        printf("ticks=%u voice_ops=%u mixed_frames=%u\n", (unsigned)stats.ticks, (unsigned)stats.voice_ops,
               (unsigned)stats.mixed_frames);
    }
    if (script != NULL) {
        const sel_state* st = sel_engine_state(e);
        printf("mute_mask=0x%08x\n", (unsigned)st->mute_mask);
        printf("end_position order=%u row=%u\n", (unsigned)st->song.pattern_pos.order,
               (unsigned)st->song.pattern_pos.row);
        printf("pitch_factor_q16=%u\n", (unsigned)(st->pitch_q16 != 0u ? st->pitch_q16 : 65536u));
    }
    if (dump != NULL) {
        fclose(dump);
        printf("viz_blocks=%u\n", (unsigned)viz_blocks);
    }
    if (o.stats != NULL && out_id != NULL) {
        const sel_tag* st = o.stats(out);
        uintptr_t writes = sel_tag_find(st, SEL_OUT_STAT_REG_WRITES, 0);
        const char* digest = (const char*)sel_tag_find(st, SEL_OUT_STAT_LOG_DIGEST, 0);
        if (digest != NULL) {
            printf("output=%s aud_writes=%u reg_log_sha256=%s\n", out_id, (unsigned)writes, digest);
        }
        if (sel_tag_find(st, SEL_OUT_STAT_AHI_RATE, 0) != 0) {
            printf("output=%s ahi_bufs=%u underruns=%u ahi_buffer_frames=%u ahi_rate=%u\n", out_id,
                   (unsigned)sel_tag_find(st, SEL_OUT_STAT_AHI_BUFS, 0), (unsigned)sel_tag_find(st, SEL_OUT_STAT_AHI_UNDERRUNS, 0),
                   (unsigned)sel_tag_find(st, SEL_OUT_STAT_AHI_BUFFER, 0), (unsigned)sel_tag_find(st, SEL_OUT_STAT_AHI_RATE, 0));
        }
        digest = (const char*)sel_tag_find(st, SEL_OUT_STAT_P14_HASH, 0);
        if (digest != NULL) {
            printf("output=%s p14_frames=%u p14_bufs=%u p14_max_err_lsb=%u p14_hash=%s\n", out_id,
                   (unsigned)sel_tag_find(st, SEL_OUT_STAT_P14_FRAMES, 0), (unsigned)sel_tag_find(st, SEL_OUT_STAT_P14_BUFS, 0),
                   (unsigned)sel_tag_find(st, SEL_OUT_STAT_P14_MAX_ERR, 0), digest);
        }
    }
    o.close(out);
    sel_engine_destroy(e);
    return (ctx.failed && !ctx.script) || pumped != total;
}
