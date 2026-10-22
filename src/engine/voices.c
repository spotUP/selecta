/* RED stub (plan row I26): the real backend arrives with I27. */
#include "mixer.h"

static void play(const sel_voices* v, uint32_t i, const sel_sample* s, uint32_t f) { (void)v; (void)i; (void)s; (void)f; }
static void queue(const sel_voices* v, uint32_t i, const sel_sample* s) { (void)v; (void)i; (void)s; }
static void stop(const sel_voices* v, uint32_t i) { (void)v; (void)i; }
static void set_u32(const sel_voices* v, uint32_t i, uint32_t x) { (void)v; (void)i; (void)x; }
static void set_pan(const sel_voices* v, uint32_t i, int32_t x) { (void)v; (void)i; (void)x; }
static int is_playing(const sel_voices* v, uint32_t i) { (void)v; (void)i; return 0; }
static void get_state(const sel_voices* v, uint32_t i, sel_voice_state* o) { (void)v; (void)i; (void)o; }

const sel_voices* sel_mixer_voices(sel_mixer* m) {
    m->api.size = sizeof(sel_voices);
    m->api.play = play;
    m->api.queue = queue;
    m->api.stop = stop;
    m->api.set_pitch_hz = set_u32;
    m->api.set_period = set_u32;
    m->api.set_volume = set_u32;
    m->api.set_pan = set_pan;
    m->api.is_playing = is_playing;
    m->api.get_state = get_state;
    m->api.ctx = m;
    return &m->api;
}
