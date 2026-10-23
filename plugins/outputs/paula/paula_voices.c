/* Paula voice sink (plan M2 phase 3, I118/I119). See paula_voices.h. */
#include "paula_voices.h"

#include <string.h>

#define NO_VOL 0xFFFFFFFFu

uint32_t sel_paula_period_from_q8(uint32_t hz_q8) {
    /* round(3546895 / (hz_q8 / 256)) = (3546895 * 256 + hz_q8 / 2) / hz_q8; 3546895 * 256 fits in 32 bits. */
    uint32_t per;
    if (hz_q8 == 0) {
        return 0;
    }
    per = (SEL_PAULA_CLOCK * 256u + hz_q8 / 2u) / hz_q8;
    if (per < SEL_PAULA_PERIOD_MIN) {
        per = SEL_PAULA_PERIOD_MIN;
    }
    if (per > SEL_PAULA_PERIOD_MAX) {
        per = SEL_PAULA_PERIOD_MAX;
    }
    return per;
}

static sel_paula* self(const sel_voices* api) {
    return (sel_paula*)api->ctx;
}

static sel_paula_chan* chan_at(const sel_voices* api, uint32_t voice) {
    return voice < SEL_PAULA_VOICES ? &self(api)->chan[voice] : NULL;
}

static uint32_t reg(uint32_t voice, uint32_t base) {
    return base + voice * SEL_PAULA_CHANNEL_STRIDE;
}

static void dma(sel_paula* p, uint32_t voice, int on) {
    p->bus.write(p->bus.ctx, SEL_PAULA_DMACON,
                 on ? (SEL_PAULA_DMA_SETCLR | SEL_PAULA_DMA_MASTER | (1u << voice)) : (1u << voice));
}

static void free_copy(sel_paula* p, sel_paula_chan* c, uint32_t slot) {
    if (c->copy[slot] != NULL) {
        p->host->mem_free(p->host, c->copy[slot]);
        c->copy[slot] = NULL;
        c->length[slot] = 0;
    }
}

/* Copies the sample into chip RAM (SEL_MEM_SAMPLE) as signed 8-bit, an even byte count. 0 on failure or no data. */
static int copy_sample(sel_paula* p, sel_paula_chan* c, uint32_t slot, const sel_sample* s) {
    uint32_t n = s->length, i, bytes;
    uint8_t* d;
    free_copy(p, c, slot);
    if (s->data == NULL || n == 0) {
        return 0;
    }
    bytes = (n + 1u) & ~1u;
    d = (uint8_t*)p->host->mem_alloc(p->host, bytes, SEL_MEM_SAMPLE);
    if (d == NULL) {
        return 0;
    }
    if (s->format == SEL_SAMPLE_S16) {
        const int16_t* src = (const int16_t*)s->data;
        for (i = 0; i < n; i++) {
            d[i] = (uint8_t)(src[i] >> 8);
        }
    } else {
        memcpy(d, s->data, n);
    }
    if (bytes != n) {
        d[n] = 0;
    }
    c->copy[slot] = d;
    c->serial[slot] = ++p->serial;
    c->length[slot] = bytes;
    return 1;
}

static void write_lc_len(sel_paula* p, uint32_t voice, const sel_paula_chan* c, uint32_t slot, uint32_t offset,
                         uint32_t bytes) {
    /* AUDxLEN is 16 bits in words; 0 would mean 65536 on some chips, so clamp a longer sample to 65535 words */
    uint32_t words = bytes / 2u;
    if (words > 65535u) {
        words = 65535u;
    }
    p->bus.set_lc(p->bus.ctx, voice, c->copy[slot], offset, c->serial[slot]);
    p->bus.write(p->bus.ctx, reg(voice, SEL_PAULA_AUD_LEN), words);
}

static void write_period(sel_paula* p, uint32_t voice, sel_paula_chan* c, uint32_t period) {
    if (period != 0 && period != c->last_period) {
        c->last_period = period;
        p->bus.write(p->bus.ctx, reg(voice, SEL_PAULA_AUD_PER), period);
    }
}

static void write_volume(sel_paula* p, uint32_t voice, sel_paula_chan* c) {
    uint32_t v = c->volume_q16 >> 10; /* 0..64 */
    if (v > 64u) {
        v = 64u;
    }
    if (v != c->last_vol) {
        c->last_vol = v;
        p->bus.write(p->bus.ctx, reg(voice, SEL_PAULA_AUD_VOL), v);
    }
}

static void p_play(const sel_voices* api, uint32_t voice, const sel_sample* s, uint32_t start_frame) {
    sel_paula* p = self(api);
    sel_paula_chan* c = chan_at(api, voice);
    uint32_t start, loop_start, loop_len;
    if (c == NULL || s == NULL) {
        return;
    }
    dma(p, voice, 0);
    free_copy(p, c, 1);
    c->playing = 0;
    if (!copy_sample(p, c, 0, s)) {
        return;
    }
    c->view = *s;
    c->view.data = c->copy[0];
    c->view.format = SEL_SAMPLE_S8;
    start = (start_frame < s->length ? start_frame : 0u) & ~1u; /* Paula fetches words */
    write_period(p, voice, c, sel_paula_period_from_q8(c->pitch_q8));
    write_volume(p, voice, c);
    write_lc_len(p, voice, c, 0, start, c->length[0] - start);
    dma(p, voice, 1);
    c->playing = 1;
    /* Paula reloads AUDxLC/AUDxLEN when the first pass ends: write the loop part now. */
    loop_start = s->loop_start & ~1u;
    loop_len = s->loop_length & ~1u;
    if (loop_len != 0 && loop_start + loop_len <= c->length[0] &&
        !(loop_start == start && loop_start + loop_len == c->length[0])) {
        write_lc_len(p, voice, c, 0, loop_start, loop_len);
    }
}

static void p_queue(const sel_voices* api, uint32_t voice, const sel_sample* s) {
    sel_paula* p = self(api);
    sel_paula_chan* c = chan_at(api, voice);
    uint32_t ls, ll;
    if (c == NULL || s == NULL) {
        return;
    }
    if (!c->playing) {
        p_play(api, voice, s, 0);
        return;
    }
    if (!copy_sample(p, c, 1, s)) {
        return;
    }
    ls = s->loop_start & ~1u;
    ll = s->loop_length & ~1u;
    if (ll != 0 && ls + ll <= c->length[1]) {
        write_lc_len(p, voice, c, 1, ls, ll);
    } else {
        write_lc_len(p, voice, c, 1, 0, c->length[1]);
    }
}

static void p_stop(const sel_voices* api, uint32_t voice) {
    sel_paula* p = self(api);
    sel_paula_chan* c = chan_at(api, voice);
    if (c == NULL) {
        return;
    }
    dma(p, voice, 0);
    c->playing = 0;
    free_copy(p, c, 0);
    free_copy(p, c, 1);
}

static void p_set_pitch_hz(const sel_voices* api, uint32_t voice, uint32_t hz_q8) {
    sel_paula_chan* c = chan_at(api, voice);
    if (c == NULL || hz_q8 == 0) {
        return;
    }
    c->pitch_q8 = hz_q8;
    write_period(self(api), voice, c, sel_paula_period_from_q8(hz_q8)); /* only when it changes */
}

static void p_set_period(const sel_voices* api, uint32_t voice, uint32_t period) {
    sel_paula_chan* c = chan_at(api, voice);
    if (c == NULL || period == 0) {
        return;
    }
    c->pitch_q8 = (SEL_PAULA_CLOCK * 256u + period / 2u) / period;
    write_period(self(api), voice, c, period);
}

static void p_set_volume(const sel_voices* api, uint32_t voice, uint32_t volume_q16) {
    sel_paula_chan* c = chan_at(api, voice);
    if (c == NULL) {
        return;
    }
    c->volume_q16 = volume_q16;
    write_volume(self(api), voice, c);
}

static void p_set_pan(const sel_voices* api, uint32_t voice, int32_t pan) {
    sel_paula_chan* c = chan_at(api, voice);
    if (c != NULL) {
        c->pan = pan; /* Paula channels are hard panned: kept for get_state only */
    }
}

static int p_is_playing(const sel_voices* api, uint32_t voice) {
    sel_paula_chan* c = chan_at(api, voice);
    return c != NULL && c->playing;
}

static void p_get_state(const sel_voices* api, uint32_t voice, sel_voice_state* out) {
    sel_paula_chan* c = chan_at(api, voice);
    memset(out, 0, sizeof *out);
    if (c == NULL) {
        return;
    }
    out->playing = c->playing;
    out->sample = c->playing ? &c->view : NULL;
    out->pitch_q8 = c->pitch_q8;
    out->volume_q16 = c->volume_q16;
    out->pan = c->pan;
}

void sel_paula_init(sel_paula* p, const sel_host* host, const sel_paula_bus* bus) {
    uint32_t i;
    memset(p, 0, sizeof *p);
    p->host = host;
    p->bus = *bus;
    for (i = 0; i < SEL_PAULA_VOICES; i++) {
        p->chan[i].last_vol = NO_VOL;
    }
    p->api.size = (uint32_t)sizeof(sel_voices);
    p->api.play = p_play;
    p->api.queue = p_queue;
    p->api.stop = p_stop;
    p->api.set_pitch_hz = p_set_pitch_hz;
    p->api.set_period = p_set_period;
    p->api.set_volume = p_set_volume;
    p->api.set_pan = p_set_pan;
    p->api.is_playing = p_is_playing;
    p->api.get_state = p_get_state;
    p->api.ctx = p;
}

void sel_paula_shutdown(sel_paula* p) {
    uint32_t i;
    for (i = 0; i < SEL_PAULA_VOICES; i++) {
        p_stop(&p->api, i);
    }
}

const sel_voices* sel_paula_api(sel_paula* p) {
    return &p->api;
}
