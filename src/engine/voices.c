/* sel_voices backend over the fixed-point mixer (design doc section 4).
 * Runs between ticks, never per sample; integer only. */
#include "mixer.h"

static sel_mixer_voice* voice_at(const sel_voices* api, uint32_t voice) {
    sel_mixer* m = (sel_mixer*)api->ctx;
    m->voice_ops++;
    return voice < m->voice_count ? &m->voices[voice] : NULL;
}

static void start(sel_mixer_voice* v, const sel_sample* s, uint32_t start_frame) {
    v->current = *s;
    v->has_queued = 0;
    v->playing = s->data != NULL;
    if (v->playing) v->viz_kick = 1;
    v->position = start_frame;
    v->frac = 0;
}

static void v_play(const sel_voices* api, uint32_t voice, const sel_sample* s, uint32_t start_frame) {
    sel_mixer_voice* v = voice_at(api, voice);
    if (v != NULL && s != NULL) start(v, s, start_frame);
}

static void v_queue(const sel_voices* api, uint32_t voice, const sel_sample* s) {
    sel_mixer_voice* v = voice_at(api, voice);
    if (v == NULL || s == NULL) return;
    if (v->playing) {
        v->queued = *s;
        v->has_queued = 1;
    } else {
        start(v, s, 0);
    }
}

static void v_stop(const sel_voices* api, uint32_t voice) {
    sel_mixer_voice* v = voice_at(api, voice);
    if (v != NULL) {
        v->playing = 0;
        v->has_queued = 0;
    }
}

static void v_set_pitch_hz(const sel_voices* api, uint32_t voice, uint32_t hz_q8) {
    sel_mixer_voice* v = voice_at(api, voice);
    if (v == NULL || hz_q8 == 0) return;
    v->pitch_q8 = hz_q8;
    v->step = sel_mixer_hz_step(hz_q8, ((sel_mixer*)api->ctx)->rate);
}

static void v_set_period(const sel_voices* api, uint32_t voice, uint32_t period) {
    sel_mixer_voice* v = voice_at(api, voice);
    uint32_t rate = ((sel_mixer*)api->ctx)->rate;
    if (v == NULL || period == 0) return;
    v->step = sel_mixer_period_step(period, rate);
    /* 24.8 Hz for get_state: clock / period, in the same integer math. */
    v->pitch_q8 = (uint32_t)((((uint64_t)SEL_PAULA_PAL_CLOCK) << 8) / period);
}

static void v_set_volume(const sel_voices* api, uint32_t voice, uint32_t volume_q16) {
    sel_mixer_voice* v = voice_at(api, voice);
    if (v != NULL) v->volume_q16 = volume_q16 > 65536u ? 65536u : volume_q16;
}

static void v_set_pan(const sel_voices* api, uint32_t voice, int32_t pan) {
    sel_mixer_voice* v = voice_at(api, voice);
    if (v != NULL) v->pan = pan < -32768 ? -32768 : (pan > 32767 ? 32767 : pan);
}

/* Mirrors nuade_voice_is_playing: a one-shot that ran off its end on the last
 * frame counts as finished, so a player does not stop it and drop a queued sample. */
static int v_is_playing(const sel_voices* api, uint32_t voice) {
    const sel_mixer_voice* v = voice_at(api, voice);
    int ran_off_end;
    if (v == NULL) return 0;
    ran_off_end = !v->has_queued && v->current.loop_length == 0 && v->position >= v->current.length;
    return v->playing && !ran_off_end;
}

static void v_get_state(const sel_voices* api, uint32_t voice, sel_voice_state* out) {
    const sel_mixer_voice* v = voice_at(api, voice);
    out->playing = 0;
    out->sample = NULL;
    out->position = 0;
    out->pitch_q8 = 0;
    out->volume_q16 = 0;
    out->pan = 0;
    if (v == NULL) return;
    out->playing = v->playing;
    out->sample = v->playing ? &v->current : NULL;
    out->position = v->playing ? v->position : 0;
    out->pitch_q8 = v->pitch_q8;
    out->volume_q16 = v->volume_q16;
    out->pan = v->pan;
}

const sel_voices* sel_mixer_voices(sel_mixer* m) {
    m->api.size = (uint32_t)sizeof(sel_voices);
    m->api.play = v_play;
    m->api.queue = v_queue;
    m->api.stop = v_stop;
    m->api.set_pitch_hz = v_set_pitch_hz;
    m->api.set_period = v_set_period;
    m->api.set_volume = v_set_volume;
    m->api.set_pan = v_set_pan;
    m->api.is_playing = v_is_playing;
    m->api.get_state = v_get_state;
    m->api.ctx = m;
    return &m->api;
}
