/* Desktop pal: no task priorities, the SPSC indices need no interrupt guard. */
#include "../pal.h"

int sel_pal_set_task_priority(int pri) {
    return pri;
}

void sel_pal_publish_begin(void) {}

void sel_pal_publish_end(void) {}
