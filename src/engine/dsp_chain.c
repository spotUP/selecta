/* The engine's DSP chain (src/engine/dsp_chain.h) and the generated
 * crossfeed coefficient table. No float: the table comes from
 * tools/gen_dsp_coef.c at build time. */
#include "dsp_chain.h"

#include "dsp_coef.h"

#include <string.h>

#define TABLE_ROWS (sizeof sel_dsp_crossfeed_table / sizeof sel_dsp_crossfeed_table[0])

int sel_dsp_crossfeed_coef_get(uint32_t rate, sel_dsp_crossfeed_coef* out) {
    size_t i, best = 0;
    uint32_t best_d = 0xFFFFFFFFu;
    if (rate == 0) {
        return 0;
    }
    for (i = 0; i < TABLE_ROWS; i++) {
        uint32_t r = (uint32_t)sel_dsp_crossfeed_table[i].rate;
        uint32_t d = r > rate ? r - rate : rate - r;
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    *out = sel_dsp_crossfeed_table[best];
    return 1;
}

void sel_dsp_crossfeed_apply(sel_dsp_param_fn param, void* state, const sel_dsp_crossfeed_coef* c) {
    param(state, SEL_DSP_CROSSFEED_P_DELAY, c->delay);
    param(state, SEL_DSP_CROSSFEED_P_LOWPASS, c->lowpass);
    param(state, SEL_DSP_CROSSFEED_P_BLEED, c->bleed);
    param(state, SEL_DSP_CROSSFEED_P_DIRECT, c->direct);
}

static sel_dsp_slot* find(sel_dsp_chain* c, const char* id) {
    uint32_t i;
    for (i = 0; i < SEL_DSP_CHAIN_SLOTS; i++) {
        if (c->slot[i].id != NULL && strcmp(c->slot[i].id, id) == 0) {
            return &c->slot[i];
        }
    }
    return NULL;
}

/* Opens the plugin of `s` at `rate` and hands it its coefficients. */
static int open_slot(sel_dsp_slot* s, const sel_host* host, uint32_t rate) {
    sel_dsp_crossfeed_coef coef;
    s->state = s->open(host, rate, 2);
    if (s->state == NULL) {
        return 0;
    }
    s->rate = rate;
    if (strcmp(s->id, "crossfeed") == 0 && sel_dsp_crossfeed_coef_get(rate, &coef)) {
        sel_dsp_crossfeed_apply(s->param, s->state, &coef);
    }
    return 1;
}

int sel_dsp_chain_enable(sel_dsp_chain* c, const sel_host* host, const char* id, const sel_tag* hooks,
                         uint32_t rate) {
    sel_dsp_slot* s = find(c, id);
    uint32_t i;
    if (s != NULL) {
        s->on = s->state != NULL;
        return s->on;
    }
    for (i = 0; i < SEL_DSP_CHAIN_SLOTS && c->slot[i].id != NULL; i++) {
    }
    if (i == SEL_DSP_CHAIN_SLOTS) {
        return 0;
    }
    s = &c->slot[i];
    s->id = id;
    s->open = (sel_dsp_open_fn)sel_tag_find(hooks, SEL_DSP_OPEN, 0);
    s->process = (sel_dsp_process_fn)sel_tag_find(hooks, SEL_DSP_PROCESS, 0);
    s->close = (sel_dsp_close_fn)sel_tag_find(hooks, SEL_DSP_CLOSE, 0);
    s->param = (sel_dsp_param_fn)sel_tag_find(hooks, SEL_DSP_PARAM, 0);
    if (!s->open || !s->process || !s->close || !s->param || !open_slot(s, host, rate)) {
        memset(s, 0, sizeof *s);
        return 0;
    }
    s->on = 1;
    return 1;
}

void sel_dsp_chain_disable(sel_dsp_chain* c, const char* id) {
    sel_dsp_slot* s = find(c, id);
    if (s != NULL) {
        s->on = 0;
    }
}

int sel_dsp_chain_is_on(const sel_dsp_chain* c, const char* id) {
    uint32_t i;
    for (i = 0; i < SEL_DSP_CHAIN_SLOTS; i++) {
        if (c->slot[i].id != NULL && c->slot[i].on && strcmp(c->slot[i].id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

void sel_dsp_chain_set_rate(sel_dsp_chain* c, const sel_host* host, uint32_t rate) {
    uint32_t i;
    for (i = 0; i < SEL_DSP_CHAIN_SLOTS; i++) {
        sel_dsp_slot* s = &c->slot[i];
        if (s->id != NULL && s->state != NULL && s->rate != rate) {
            s->on = 0;
            s->close(s->state);
            s->state = NULL;
            if (open_slot(s, host, rate)) {
                s->on = 1;
            }
        }
    }
}

void sel_dsp_chain_free(sel_dsp_chain* c) {
    uint32_t i;
    for (i = 0; i < SEL_DSP_CHAIN_SLOTS; i++) {
        sel_dsp_slot* s = &c->slot[i];
        s->on = 0;
        if (s->state != NULL) {
            s->close(s->state);
        }
        memset(s, 0, sizeof *s);
    }
}

void sel_dsp_chain_process(sel_dsp_chain* c, int16_t* stereo, uint32_t frames) {
    uint32_t i, ran = 0;
    for (i = 0; i < SEL_DSP_CHAIN_SLOTS; i++) {
        sel_dsp_slot* s = &c->slot[i];
        if (s->on && s->state != NULL) {
            s->process(s->state, stereo, frames);
            ran = 1;
        }
    }
    c->blocks += ran;
}
