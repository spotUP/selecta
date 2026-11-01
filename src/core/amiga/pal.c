/* Amiga pal: SetTaskPri on the calling task, Disable()/Enable() around the
 * SPSC head/tail publication (interrupt-safe, a few instructions long), and
 * dos Delay() for sleeps (whole 1/50 s ticks). */
#include "../pal.h"

#include <exec/tasks.h>
#include <proto/dos.h>
#include <proto/exec.h>

int sel_pal_set_task_priority(int pri) {
    struct Task* me = FindTask(NULL);

    SetTaskPri(me, (BYTE)pri);
    return (int)me->tc_Node.ln_Pri;
}

void sel_pal_publish_begin(void) {
    Disable();
}

void sel_pal_publish_end(void) {
    Enable();
}

unsigned sel_pal_sleep_ms(unsigned ms) {
    unsigned ticks = (ms + 19u) / 20u;

    if (ticks > 0u) {
        Delay((LONG)ticks);
    }
    return ticks * 20u;
}
