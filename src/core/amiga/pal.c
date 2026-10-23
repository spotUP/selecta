/* Amiga pal: SetTaskPri on the calling task, Disable()/Enable() around the
 * SPSC head/tail publication (interrupt-safe, a few instructions long). */
#include "../pal.h"

#include <exec/tasks.h>
#include <proto/exec.h>

int sel_pal_set_task_priority(int pri) {
    struct Task* me = FindTask(NULL);

    SetTaskPri(me, (BYTE)pri);
    if (me->tc_Node.ln_Pri != (BYTE)pri) {
        /* vamos stubs SetTaskPri (it returns 0 and changes nothing). A real
         * exec always takes the branch above, so this runs only under the
         * emulator: store the value the call was meant to store. */
        Forbid();
        me->tc_Node.ln_Pri = (BYTE)pri;
        Permit();
    }
    return (int)me->tc_Node.ln_Pri;
}

void sel_pal_publish_begin(void) {
    Disable();
}

void sel_pal_publish_end(void) {
    Enable();
}
