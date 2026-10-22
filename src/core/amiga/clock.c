/* Amiga clock: dos.library DateStamp, 50 ticks per second. The M2 Amiga
 * host replaces this with timer.device ReadEClock for finer resolution.
 * libnix's clock() opens timer.device at startup, which vamos cannot
 * emulate, so the Amiga build never links it. */
#include "host_internal.h"

#include <proto/dos.h>

uint32_t sel_host_clock_ms(const sel_host* h) {
    struct DateStamp ds;
    (void)h;
    DateStamp(&ds);
    return (uint32_t)ds.ds_Days * 86400000u + (uint32_t)ds.ds_Minute * 60000u +
           (uint32_t)ds.ds_Tick * 20u;
}
