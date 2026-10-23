#define _POSIX_C_SOURCE 200809L
#include "host_internal.h"

#include <selecta/machine.h>

#include <time.h>

const char* sel_clock_source(void) {
    return "monotonic";
}

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

/* Local wall time as milliseconds since midnight (HOSTQ_LOCAL_TIME). */
uint32_t sel_host_local_ms(const sel_host* h) {
    time_t now = time(NULL);
    struct tm tmv;
    (void)h;
    if (localtime_r(&now, &tmv) == NULL) {
        return 0;
    }
    return ((uint32_t)tmv.tm_hour * 3600u + (uint32_t)tmv.tm_min * 60u + (uint32_t)tmv.tm_sec) * 1000u;
}
