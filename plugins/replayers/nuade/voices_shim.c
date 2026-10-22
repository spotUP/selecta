/* The nuade voice API implemented over sel_voices. This is the one place
 * float meets the integer voice API: nuade passes double/float, each call
 * converts once to Hz 24.8, volume Q16 and pan -32768..32767. Desktop only;
 * the Amiga build gets a fixed-point adapter in Phase 5 (D-M2-1). */
#include "shim.h"

static uint32_t to_q8(double hz) {
    double v = hz * 256.0 + 0.5;
    if (!(hz > 0.0)) {
        return 0;
    }
    return v >= 4294967295.0 ? 0xFFFFFFFFu : (uint32_t)v;
}

static uint32_t to_q16(float vol) {
    if (!(vol > 0.0f)) {
        return 0;
    }
    return vol >= 1.0f ? 65536u : (uint32_t)(vol * 65536.0f + 0.5f);
}

static int32_t to_pan(float pan) {
    if (pan <= -1.0f) {
        return -32768;
    }
    if (pan >= 1.0f) {
        return 32767;
    }
    return (int32_t)(pan * 32767.0f);
}

static void to_sel(const NuadeSample* in, sel_sample* out) {
    out->data = in->data;
    out->format = (uint32_t)in->format;
    out->length = in->length;
    out->loop_start = in->loop_start;
    out->loop_length = in->loop_length;
}

static int valid(const NuadeVoices* v, uint32_t voice) {
    return voice < SEL_NUADE_MAX_VOICES && v->api != NULL;
}

void nuade_voice_play(NuadeVoices* v, uint32_t voice, const NuadeSample* sample, uint32_t start_frame) {
    sel_sample s;
    if (!valid(v, voice)) return;
    v->current[voice] = *sample;
    to_sel(sample, &s);
    v->api->play(v->api, voice, &s, start_frame);
}

void nuade_voice_queue(NuadeVoices* v, uint32_t voice, const NuadeSample* sample) {
    sel_sample s;
    if (!valid(v, voice)) return;
    to_sel(sample, &s);
    v->api->queue(v->api, voice, &s);
}

void nuade_voice_stop(NuadeVoices* v, uint32_t voice) {
    if (!valid(v, voice)) return;
    v->api->stop(v->api, voice);
}

void nuade_voice_set_pitch(NuadeVoices* v, uint32_t voice, double hz) {
    if (!valid(v, voice)) return;
    v->api->set_pitch_hz(v->api, voice, to_q8(hz));
}

void nuade_voice_set_volume(NuadeVoices* v, uint32_t voice, float volume) {
    if (!valid(v, voice)) return;
    v->api->set_volume(v->api, voice, to_q16(volume));
}

void nuade_voice_set_pan(NuadeVoices* v, uint32_t voice, float pan) {
    if (!valid(v, voice)) return;
    v->api->set_pan(v->api, voice, to_pan(pan));
}

bool nuade_voice_is_playing(const NuadeVoices* v, uint32_t voice) {
    if (!valid(v, voice)) return false;
    return v->api->is_playing(v->api, voice) != 0;
}

void nuade_voice_get_state(const NuadeVoices* v, uint32_t voice, NuadeVoiceState* st) {
    sel_voice_state s;
    st->playing = false;
    st->sample = NULL;
    st->position = 0.0;
    st->pitch_hz = 0.0;
    st->volume = 0.0f;
    st->pan = 0.0f;
    st->wraps = 0;
    st->event_frame = 0;
    st->frame = 0;
    if (!valid(v, voice)) return;
    v->api->get_state(v->api, voice, &s);
    st->playing = s.playing != 0;
    st->sample = s.playing ? &v->current[voice] : NULL;
    st->position = (double)s.position;
    st->pitch_hz = (double)s.pitch_q8 / 256.0;
    st->volume = (float)s.volume_q16 / 65536.0f;
    st->pan = (float)s.pan / 32768.0f;
}

bool nuade_voice_muted(const NuadeVoices* v, uint32_t track) {
    (void)v;
    (void)track;
    return false; /* mutes live in the engine (design doc section 4) */
}

bool nuade_instrument_muted(const NuadeVoices* v, int32_t instrument) {
    (void)v;
    (void)instrument;
    return false;
}
