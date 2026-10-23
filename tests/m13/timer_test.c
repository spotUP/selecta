/* T45 unit side: the ramp shapes of the sleep timer and alarm (src/engine/timer.c).
 * The scripted T45 proves the engine runs them; this proves the numbers. */
#include "../../src/engine/timer.h"

#include <stdio.h>
#include <stdlib.h>

#define RATE 44100u
#define BLOCK 441u

static int failures;

static void check(int ok, const char* what) {
    if (!ok) {
        fprintf(stderr, "timer_test: FAIL %s\n", what);
        failures++;
    }
}

/* Renders `frames` of constant 10000 and returns the last sample. */
static int16_t run(sel_timer* t, uint32_t frames) {
    int16_t buf[BLOCK * 2u];
    int16_t last = 0;
    while (frames > 0u) {
        uint32_t n = frames < BLOCK ? frames : BLOCK, i;
        for (i = 0; i < n * 2u; i++) {
            buf[i] = 10000;
        }
        sel_timer_apply(t, buf, n);
        last = buf[n * 2u - 1u];
        frames -= n;
    }
    return last;
}

int main(void) {
    sel_timer t;
    int16_t v;
    /* sleep: armed at 100000 ms of day, silent until the minute is up */
    sel_timer_init(&t);
    sel_timer_set_sleep(&t, 60000u, 100000u);
    sel_timer_poll(&t, 159999u, 1, RATE * 10u);
    check(t.sleep_expired == 0 && t.ramp_dir == SEL_RAMP_NONE, "not expired at 59.999 s");
    sel_timer_poll(&t, 160000u, 1, RATE * 10u);
    check(t.sleep_expired == 1 && t.ramp_dir == SEL_RAMP_DOWN, "expired at 60 s starts the down ramp");
    v = run(&t, RATE * 5u);
    check(v > 4500 && v < 5500, "gain is one half after 5000 ms");
    check(t.sleep_done == 0, "not done at the midpoint");
    v = run(&t, RATE * 5u);
    check(v < 10, "silent at 10000 ms");
    check(t.sleep_done == 1, "done after 10000 ms");
    sel_timer_ramp_clear(&t);
    v = run(&t, BLOCK);
    check(v == 10000, "volume restored after the ramp is cleared");
    /* cancel */
    sel_timer_set_sleep(&t, 60000u, 0u);
    sel_timer_poll(&t, 1u, 1, RATE * 10u);
    sel_timer_set_sleep(&t, 0u, 2u);
    sel_timer_poll(&t, 70000u, 1, RATE * 10u);
    check(t.ramp_dir == SEL_RAMP_NONE, "SLEEP_TIMER 0 cancels");
    /* midnight wrap */
    sel_timer_init(&t);
    sel_timer_set_sleep(&t, 120000u, SEL_DAY_MS - 60000u);
    sel_timer_poll(&t, 59999u, 1, RATE);
    check(t.sleep_expired == 0, "wrap: not yet");
    sel_timer_poll(&t, 60000u, 1, RATE);
    check(t.sleep_expired == 1, "wrap: expires across midnight");
    /* alarm: 07:30 fires on the second after 07:29:59, then again the next day */
    sel_timer_init(&t);
    sel_timer_set_alarm(&t, 27000000u, 26999000u);
    sel_timer_poll(&t, 26999500u, 0, 0);
    check(t.alarm_fired == 0, "alarm not before the time");
    sel_timer_poll(&t, 27000000u, 0, 0);
    check(t.alarm_fired == 1, "alarm fires at 07:30:00");
    t.alarm_fired = 0;
    sel_timer_poll(&t, 27001000u, 0, 0);
    check(t.alarm_fired == 0, "alarm fires once");
    sel_timer_poll(&t, 26999000u, 0, 0); /* a full day later is the same clock reading */
    sel_timer_poll(&t, 27000000u, 0, 0);
    check(t.alarm_fired == 1, "alarm re-arms daily");
    t.alarm_fired = 0;
    sel_timer_set_alarm(&t, SEL_ALARM_OFF, 27000000u);
    sel_timer_poll(&t, 27000500u, 0, 0);
    sel_timer_poll(&t, 26999000u, 0, 0);
    sel_timer_poll(&t, 27000000u, 0, 0);
    check(t.alarm_fired == 0, "ALARM off cancels");
    /* alarm ramp: 0 to full over 30000 ms */
    sel_timer_ramp_up(&t, RATE * 30u);
    v = run(&t, BLOCK);
    check(v < 20, "up ramp starts at silence");
    v = run(&t, RATE * 15u - BLOCK);
    check(v > 4500 && v < 5500, "up ramp is one half after 15000 ms");
    v = run(&t, RATE * 15u);
    check(v > 9900, "up ramp reaches the set volume");
    check(t.ramp_dir == SEL_RAMP_NONE, "up ramp ends");
    if (failures == 0) {
        puts("timer_test: ok");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
