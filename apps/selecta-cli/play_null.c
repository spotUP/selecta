/* play-null --rate <hz> --frames <n> (T4): the null output pulls frames
 * through the engine render, found through the registry like any output.
 * SEL_ALLOC_FREEZE on stderr marks the point after which the render path
 * must not allocate, until SEL_ALLOC_THAW (no-alloc test, plan Phase 5). */
#include <selecta/engine.h>
#include <selecta/output.h>
#include <selecta/registry.h>

#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t engine_pull(void* ctx, int16_t* stereo, uint32_t frames) {
    return sel_engine_render((sel_engine*)ctx, stereo, frames);
}

int sel_cli_play_null(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    unsigned long rate = 44100, frames = 0;
    const sel_tag* desc;
    const sel_tag* hooks;
    sel_out_open_fn out_open;
    sel_out_close_fn out_close;
    sel_out_set_source_fn out_set_source;
    sel_out_pump_fn out_pump;
    sel_engine* e;
    void* out;
    uint32_t pulled = 0;
    int i;

    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--rate") == 0) {
            rate = strtoul(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--frames") == 0) {
            frames = strtoul(argv[i + 1], NULL, 10);
        } else {
            fprintf(stderr, "play-null: unknown option %s\n", argv[i]);
            return 2;
        }
    }

    sel_registry_init(host);
    desc = sel_registry_find(SEL_PLUGIN_KIND_OUTPUT, "null");
    hooks = desc ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    if (hooks == NULL) {
        fputs("play-null: null output not registered\n", stderr);
        return 1;
    }
    out_open = (sel_out_open_fn)sel_tag_find(hooks, SEL_OUT_OPEN, 0);
    out_close = (sel_out_close_fn)sel_tag_find(hooks, SEL_OUT_CLOSE, 0);
    out_set_source = (sel_out_set_source_fn)sel_tag_find(hooks, SEL_OUT_SET_SOURCE, 0);
    out_pump = (sel_out_pump_fn)sel_tag_find(hooks, SEL_OUT_PUMP, 0);
    if (!out_open || !out_close || !out_set_source || !out_pump) {
        fputs("play-null: null output lacks a hook\n", stderr);
        return 1;
    }

    e = sel_engine_create(host, NULL);
    if (e == NULL) {
        fputs("play-null: engine allocation failed\n", stderr);
        return 1;
    }
    out = out_open(host, (uint32_t)rate, 2);
    if (out == NULL) {
        sel_engine_destroy(e);
        fputs("play-null: output open failed\n", stderr);
        return 1;
    }
    out_set_source(out, engine_pull, e);

    fputs("SEL_ALLOC_FREEZE\n", stderr);
    fflush(stderr);
    while (pulled < frames) {
        uint32_t want = (uint32_t)frames - pulled;
        uint32_t got;
        if (want > 512u) {
            want = 512u;
        }
        got = out_pump(out, want);
        if (got == 0) {
            break;
        }
        pulled += got;
    }
    fputs("SEL_ALLOC_THAW\n", stderr);
    fflush(stderr);

    printf("pulled=%u render_calls=%u rate=%lu\n", (unsigned)pulled,
           (unsigned)sel_engine_render_calls(e), rate);
    out_close(out);
    sel_engine_destroy(e);
    return pulled == frames ? 0 : 1;
}
