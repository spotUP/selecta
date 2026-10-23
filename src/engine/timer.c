#include "timer.h"

#include <string.h>

#define REQ_SLEEP 1u
#define REQ_ALARM 2u
#define RAMP_CHUNK 256u

void sel_timer_init(sel_timer* t) {
    memset(t, 0, sizeof *t);
    t->alarm_ms = SEL_ALARM_OFF;
}

static uint32_t since(uint32_t now, uint32_t then) {
    return (now + SEL_DAY_MS - then) % SEL_DAY_MS;
}

void sel_timer_set_sleep(sel_timer* t, uint32_t ms, uint32_t now) {
    t->req_sleep_ms = ms;
    t->req_now = now;
    t->req |= REQ_SLEEP;
}

void sel_timer_set_alarm(sel_timer* t, uint32_t ms, uint32_t now) {
    t->req_alarm_ms = ms;
    t->req_now = now;
    t->req |= REQ_ALARM;
}

void sel_timer_poll(sel_timer* t, uint32_t now, int playing, uint32_t down_frames) {
    uint32_t req = t->req;
    if (req & REQ_SLEEP) {
        t->sleep_left_ms = t->req_sleep_ms;
        t->sleep_last = t->req_now;
    }
    if (req & REQ_ALARM) {
        t->alarm_ms = t->req_alarm_ms;
        t->alarm_prev = t->req_now;
        /* The alarm is minute-granular: setting it inside its own minute fires it. */
        if (t->alarm_ms != SEL_ALARM_OFF && t->alarm_ms / 60000u == t->req_now / 60000u) {
            t->alarm_fired = 1;
        }
    }
    if (req) {
        t->req &= ~req;
    }
    if (t->sleep_left_ms != 0u) {
        uint32_t el = since(now, t->sleep_last);
        t->sleep_last = now;
        if (el >= t->sleep_left_ms) {
            t->sleep_left_ms = 0;
            t->sleep_expired = 1;
            if (playing && down_frames != 0u) {
                t->ramp_pos = 0;
                t->ramp_total = down_frames;
                t->ramp_dir = SEL_RAMP_DOWN;
            } else {
                t->sleep_done = 1;
            }
        } else {
            t->sleep_left_ms -= el;
        }
    }
    if (t->alarm_ms != SEL_ALARM_OFF) {
        uint32_t el = since(now, t->alarm_prev);
        uint32_t d = since(t->alarm_ms, t->alarm_prev);
        t->alarm_prev = now;
        if (d != 0u && d <= el) {
            t->alarm_fired = 1;
        }
    }
}

/* Q16 gain of the ramp at frame `pos`. */
static uint32_t ramp_gain(const sel_timer* t, uint32_t pos) {
    uint32_t g = (uint32_t)(((uint64_t)pos << 16) / t->ramp_total);
    return t->ramp_dir == SEL_RAMP_DOWN ? 65536u - g : g;
}

void sel_timer_apply(sel_timer* t, int16_t* stereo, uint32_t frames) {
    uint32_t dir = t->ramp_dir;
    uint32_t done = 0;
    if (dir == SEL_RAMP_NONE || t->ramp_total == 0u) {
        return;
    }
    while (done < frames) {
        uint32_t n = frames - done < RAMP_CHUNK ? frames - done : RAMP_CHUNK;
        uint32_t g = t->ramp_pos >= t->ramp_total ? (dir == SEL_RAMP_DOWN ? 0u : 65536u) : ramp_gain(t, t->ramp_pos);
        uint32_t i;
        if (g != 65536u) {
            for (i = 0; i < n * 2u; i++) {
                stereo[(done * 2u) + i] = (int16_t)(((int32_t)stereo[(done * 2u) + i] * (int32_t)g) >> 16);
            }
        }
        done += n;
        t->ramp_pos += n;
    }
    if (t->ramp_pos >= t->ramp_total) {
        if (dir == SEL_RAMP_DOWN) {
            t->sleep_done = 1; /* the pump pauses and clears the ramp */
        } else {
            t->ramp_dir = SEL_RAMP_NONE;
        }
    }
}

void sel_timer_ramp_up(sel_timer* t, uint32_t frames) {
    t->ramp_dir = SEL_RAMP_NONE;
    t->ramp_pos = 0;
    t->ramp_total = frames;
    t->ramp_dir = frames != 0u ? SEL_RAMP_UP : SEL_RAMP_NONE;
}

void sel_timer_ramp_clear(sel_timer* t) {
    t->ramp_dir = SEL_RAMP_NONE;
}
