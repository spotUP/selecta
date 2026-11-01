/* Desktop pal: no task priorities, the SPSC indices need no interrupt guard. */
#include "../pal.h"

#include <time.h>

int sel_pal_set_task_priority(int pri) {
    return pri;
}

void sel_pal_publish_begin(void) {}

void sel_pal_publish_end(void) {}

unsigned sel_pal_sleep_ms(unsigned ms) {
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    if (ms > 0u) {
        nanosleep(&ts, NULL);
    }
    return ms;
}
