/* Tick scheduler (design doc section 4). A replayer reports each tick's
 * length in its own timebase (for nuade, CIA ticks at timebase_hz); the
 * scheduler converts it to whole output frames and carries the rounding
 * remainder, so the long-run frame count is exact. Integer only. */
#ifndef SELECTA_SCHEDULER_H
#define SELECTA_SCHEDULER_H

#include <selecta/host.h>

SEL_EXTERN_C_BEGIN

typedef struct sel_scheduler {
    uint32_t timebase_hz;
    uint32_t rate;
    uint64_t remainder;
} sel_scheduler;

void sel_scheduler_init(sel_scheduler* s, uint32_t timebase_hz, uint32_t rate);

/* Output frames for a tick of `length` timebase units:
 * total = remainder + length * rate; frames = total / timebase_hz;
 * remainder = total % timebase_hz. */
uint32_t sel_scheduler_frames(sel_scheduler* s, uint32_t length);

SEL_EXTERN_C_END

#endif /* SELECTA_SCHEDULER_H */
