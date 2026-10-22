/* play <song> --output <id> --seconds <n> [--rate <hz>] [--resample <name>]
 * (T16): drives a real output (coreaudio) from the engine. The output owns
 * the clock; OUT_PUMP blocks until the requested frames have been played.
 * Prints `callbacks=K frames=F` from the output's own stats. Exit 77 when the
 * output cannot open because the machine has no output device. */
#include <selecta/engine.h>
#include <selecta/output.h>
#include <selecta/registry.h>

#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Runs on the output's audio thread: the first call marks it. */
static uint32_t audio_pull(void* ctx, int16_t* stereo, uint32_t frames) {
    static int marked;
    if (!marked) {
        sel_engine_mark_audio_thread((sel_engine*)ctx);
        marked = 1;
    }
    return sel_cli_engine_pull(ctx, stereo, frames);
}

int sel_cli_play(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    sel_cli_output o;
    sel_engine* e;
    void* out;
    const char* id = NULL;
    unsigned long rate = 44100, seconds = 10;
    uint32_t resampler = 1; /* linear */
    uint32_t total, played = 0, callbacks = 0, frames = 0;
    const sel_tag* st;
    int i;

    if (argc < 1) {
        fputs("play: song path missing\n", stderr);
        return 2;
    }
    for (i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--output") == 0) {
            id = argv[i + 1];
        } else if (strcmp(argv[i], "--seconds") == 0) {
            seconds = strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--rate") == 0) {
            rate = strtoul(argv[i + 1], NULL, 10);
        } else {
            fprintf(stderr, "play: unknown option %s\n", argv[i]);
            return 2;
        }
    }
    if (id == NULL || rate == 0) {
        fputs("play: --output <id> and a nonzero --rate are required\n", stderr);
        return 2;
    }

    sel_registry_init(host);
    if (!sel_cli_find_output("play", id, &o) || o.stats == NULL) {
        return 1;
    }
    e = sel_cli_start_song("play", host, argv[0], (uint32_t)rate, resampler);
    if (e == NULL) {
        return 1;
    }
    out = o.open(host, (uint32_t)rate, 2);
    if (out == NULL) {
        sel_engine_destroy(e);
        fputs("play: no output device\n", stderr);
        return 77;
    }
    o.set_source(out, audio_pull, e);

    total = (uint32_t)(seconds * rate);
    while (played < total) {
        uint32_t got = o.pump(out, total - played);
        if (got == 0) {
            break;
        }
        played += got;
    }
    for (st = o.stats(out); st->tag != SEL_TAG_END; st++) {
        if (st->tag == SEL_OUT_STAT_FRAMES) {
            frames = (uint32_t)st->value;
        } else if (st->tag == SEL_OUT_STAT_CALLBACKS) {
            callbacks = (uint32_t)st->value;
        }
    }
    printf("callbacks=%u frames=%u\n", (unsigned)callbacks, (unsigned)frames);
    o.close(out);
    sel_engine_destroy(e);
    return played == total ? 0 : 1;
}
