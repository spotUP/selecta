/* Shared open/start path of the commands that drive an output from the
 * engine (render, play): find an output plugin by id, build an engine, open
 * and start a song. */
#include <selecta/engine.h>
#include <selecta/output.h>
#include <selecta/registry.h>

#include "cli.h"

#include <stdio.h>
#include <string.h>

uint32_t sel_cli_engine_pull(void* ctx, int16_t* stereo, uint32_t frames) {
    return sel_engine_render((sel_engine*)ctx, stereo, frames);
}

int sel_cli_find_output(const char* who, const char* id, sel_cli_output* o) {
    const sel_tag* desc = sel_registry_find(SEL_PLUGIN_KIND_OUTPUT, id);
    const sel_tag* hooks = desc ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    if (hooks == NULL) {
        fprintf(stderr, "%s: %s output not registered\n", who, id);
        return 0;
    }
    o->hooks = hooks;
    o->open = (sel_out_open_fn)sel_tag_find(hooks, SEL_OUT_OPEN, 0);
    o->close = (sel_out_close_fn)sel_tag_find(hooks, SEL_OUT_CLOSE, 0);
    o->set_source = (sel_out_set_source_fn)sel_tag_find(hooks, SEL_OUT_SET_SOURCE, 0);
    o->pump = (sel_out_pump_fn)sel_tag_find(hooks, SEL_OUT_PUMP, 0);
    o->stats = (sel_out_stats_fn)sel_tag_find(hooks, SEL_OUT_STATS, 0);
    if (!o->open || !o->close || !o->set_source || !o->pump) {
        fprintf(stderr, "%s: %s output lacks a hook\n", who, id);
        return 0;
    }
    return 1;
}

static void start_sink(void* ctx, const sel_message* evt) {
    if (evt->id == SEL_EVT_ERROR) {
        fprintf(stderr, "error %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
        *(int*)ctx = 1;
    }
}

static const char* g_rp_global;
static const char* g_rp_song;

void sel_cli_set_rp_config(const char* global, const char* song) {
    g_rp_global = global;
    g_rp_song = song;
}

sel_engine* sel_cli_start_song_sink(const char* who, const sel_host* host, const char* song, uint32_t rate,
                                    uint32_t resampler, const sel_voices* sink) {
    sel_engine_config cfg;
    sel_message msg;
    sel_engine* e;
    int failed = 0;

    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        fprintf(stderr, "%s: engine allocation failed\n", who);
        return NULL;
    }
    sel_engine_set_mix(e, rate, resampler);
    if (sink != NULL) {
        sel_engine_set_voice_sink(e, sink);
    }

    if (g_rp_global != NULL) {
        memset(&msg, 0, sizeof msg);
        msg.kind = SEL_MSG_CMD;
        msg.id = SEL_CMD_PREF_SET;
        msg.argc = 2;
        msg.args[0].type = SEL_ARG_STR;
        msg.args[0].v.s = "rp.config";
        msg.args[1].type = SEL_ARG_STR;
        msg.args[1].v.s = g_rp_global;
        sel_engine_send(e, &msg);
    }
    sel_engine_set_song_config(e, g_rp_song);
    memset(&msg, 0, sizeof msg);
    msg.kind = SEL_MSG_CMD;
    msg.id = SEL_CMD_OPEN;
    msg.argc = 1;
    msg.args[0].type = SEL_ARG_STR;
    msg.args[0].v.s = song;
    sel_engine_send(e, &msg);
    memset(&msg, 0, sizeof msg);
    msg.kind = SEL_MSG_CMD;
    msg.id = SEL_CMD_PLAY;
    sel_engine_send(e, &msg);
    sel_engine_pump(e, start_sink, &failed);
    if (failed) {
        sel_engine_destroy(e);
        return NULL;
    }
    return e;
}

sel_engine* sel_cli_start_song(const char* who, const sel_host* host, const char* song, uint32_t rate,
                               uint32_t resampler) {
    return sel_cli_start_song_sink(who, host, song, rate, resampler, NULL);
}
