#include "host_internal.h"

#include <time.h>

/* Milliseconds of wall time from a monotonic clock. clock() is process CPU
 * time: it stands still while the process sleeps (a device-paced shell turn,
 * a keys-file `wait`), so it is only the fallback where no monotonic clock
 * exists. Wraps at 2^32 ms; callers subtract. */
uint32_t sel_host_clock_ms(const sel_host* h) {
    (void)h;
#if defined(CLOCK_MONOTONIC)
    {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
            return (uint32_t)((unsigned long)ts.tv_sec * 1000ul + (unsigned long)ts.tv_nsec / 1000000ul);
        }
    }
#endif
    return (uint32_t)((unsigned long)clock() * 1000ul / (unsigned long)CLOCKS_PER_SEC);
}
