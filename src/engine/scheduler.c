#include "scheduler.h"

void sel_scheduler_init(sel_scheduler* s, uint32_t timebase_hz, uint32_t rate) {
    s->timebase_hz = timebase_hz ? timebase_hz : 1u;
    s->rate = rate;
    s->remainder = 0;
}

uint32_t sel_scheduler_frames(sel_scheduler* s, uint32_t length) {
    uint64_t total = s->remainder + (uint64_t)length * s->rate;
    uint32_t frames = (uint32_t)(total / s->timebase_hz);
    s->remainder = total % s->timebase_hz;
    return frames;
}
