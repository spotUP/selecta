/* STUB for the red tests of plan 2026-10-25-m4-arexx (M4-AX-0-1, M4-AX-0-2): answers every line with an error. */
#include "ctlexec.h"
#include <stdio.h>

int sel_ctl_exec_fwd(sel_engine* e, const char* line, char* reply, size_t cap, int* rc, sel_event_sink fwd,
                     void* fwd_ctx) {
    (void)e;
    (void)line;
    (void)fwd;
    (void)fwd_ctx;
    snprintf(reply, cap, "not_implemented");
    *rc = 10;
    return 0;
}

int sel_ctl_exec(sel_engine* e, const char* line, char* reply, size_t cap, int* rc) {
    return sel_ctl_exec_fwd(e, line, reply, cap, rc, NULL, NULL);
}
