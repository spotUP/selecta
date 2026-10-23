/* Sleep timer and alarm (plan M1.3 I96, T45). The pump thread requests, the
 * audio thread counts: wall time comes from HOSTQ_LOCAL_TIME (milliseconds
 * since local midnight, polled once per render call) and the volume ramps run
 * on rendered frames, so a fake clock and an offline render agree. Integer
 * only, no allocation. */
#ifndef SEL_ENGINE_TIMER_H
#define SEL_ENGINE_TIMER_H

#include <stdint.h>

#define SEL_DAY_MS 86400000u
#define SEL_ALARM_OFF 0xFFFFFFFFu
#define SEL_SLEEP_RAMP_MS 10000u
#define SEL_ALARM_RAMP_MS 30000u

enum { SEL_RAMP_NONE = 0, SEL_RAMP_DOWN = 1, SEL_RAMP_UP = 2 };

typedef struct sel_timer {
    /* pump -> audio request; the bit is set last and cleared by the audio side */
    volatile uint32_t req;
    uint32_t req_sleep_ms, req_alarm_ms, req_now;
    /* audio thread */
    uint32_t sleep_left_ms; /* 0: no timer */
    uint32_t sleep_last;
    uint32_t alarm_ms; /* time of day in ms, or SEL_ALARM_OFF */
    uint32_t alarm_prev;
    uint32_t ramp_pos, ramp_total;
    volatile uint32_t ramp_dir; /* SEL_RAMP_*; set by either thread, see timer.c */
    /* audio -> pump */
    volatile uint32_t sleep_done; /* the down ramp reached silence */
    volatile uint32_t sleep_expired;
    volatile uint32_t alarm_fired;
} sel_timer;

void sel_timer_init(sel_timer* t);
/* Pump: arm (minutes > 0) or cancel the sleep timer; `now` is the clock at the call. */
void sel_timer_set_sleep(sel_timer* t, uint32_t ms, uint32_t now);
/* Pump: arm the alarm for a time of day in ms, or SEL_ALARM_OFF. */
void sel_timer_set_alarm(sel_timer* t, uint32_t ms, uint32_t now);
/* Audio, once per render call: absorbs requests, counts time. A sleep expiry
 * while `playing` starts the down ramp of `down_frames`. */
void sel_timer_poll(sel_timer* t, uint32_t now, int playing, uint32_t down_frames);
/* Audio: scales `frames` stereo frames by the running ramp. */
void sel_timer_apply(sel_timer* t, int16_t* stereo, uint32_t frames);
/* Pump: starts the up ramp from silence. */
void sel_timer_ramp_up(sel_timer* t, uint32_t frames);
/* Pump: drops any ramp (volume back to the set level). */
void sel_timer_ramp_clear(sel_timer* t);

#endif
