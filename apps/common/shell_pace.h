/* I684c: the one rule for how long a shell turn lets the UI wait. Every turn blocks about SHELL_TURN_MS in exactly one
 * place: the output's pump when that output paces itself, else the UI wait. wait(0) is right only for an open output
 * whose pump blocks (AHI waits on its buffers; a second block would underrun). A clockless output, no output at all
 * (the handover to a HW-kind replayer) and an output whose pump never blocks (SEL_OUT_UNPACED, Paula) need the UI wait,
 * or the loop busy-spins. */
#ifndef SELECTA_SHELL_PACE_H
#define SELECTA_SHELL_PACE_H

#include <stdint.h>

static inline uint32_t sel_shell_wait_ms(int clockless, int have_output, int paced, uint32_t turn_ms) {
    return (clockless || !have_output || !paced) ? turn_ms : 0u;
}

#endif
