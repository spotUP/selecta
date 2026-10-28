/* I623 regression: a Ctrl-C that a bsdsocket wait swallows must survive the net layer.
 * bsdsocket's default break mask is SIGBREAKF_CTRL_C; WaitSelect returns EINTR and clears the
 * bit. Selecta's networking runs inline on the main task, so a cleared bit means the UI's poll
 * never sees the break and Selecta does not quit. Run on a rig by rig-i623-net-break.
 *
 *   net_break pending  : break already pending before sel_net_open
 *   net_break wait     : break sent by the test (Break <pid> C) during the 10 s connect wait
 *
 * Prints `net_break mode=<m> rc=<sel_net_open result> ms=<elapsed> ctrl_c_kept=<0|1>`.
 * The target 10.255.255.1 never answers, so without a break the connect waits its full timeout. */
#include "net/net.h"

#include <dos/dos.h>
#include <exec/types.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>

void __chkabort(void) {} /* the libnix Ctrl-C abort would end the probe before it reports */

int main(int argc, char** argv) {
    int pending = argc > 1 && strcmp(argv[1], "pending") == 0;
    unsigned long t0, t1;
    int rc;
    ULONG kept;
    SetSignal(0, SIGBREAKF_CTRL_C);
    if (pending) {
        SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);
    }
    printf("net_break started mode=%s\n", pending ? "pending" : "wait");
    fflush(stdout);
    t0 = sel_net_now_ms();
    rc = sel_net_open("10.255.255.1", 80, 10000);
    t1 = sel_net_now_ms();
    kept = SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C;
    if (rc >= 0) {
        sel_net_close(rc);
    }
    printf("net_break mode=%s rc=%d ms=%lu ctrl_c_kept=%d\n", pending ? "pending" : "wait", rc, t1 - t0,
           kept != 0 ? 1 : 0);
    return 0;
}
