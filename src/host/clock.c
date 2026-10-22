#include "host_internal.h"

#include <time.h>

uint32_t sel_host_clock_ms(const sel_host* h) {
    (void)h;
    return (uint32_t)((unsigned long)clock() * 1000ul / (unsigned long)CLOCKS_PER_SEC);
}
