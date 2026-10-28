/* Console for the text UI fallback (I671b). See console_fallback.h. */
#include "console_fallback.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef __AMIGA__
/* The host text UI runs on pipes as well as on a terminal (scripts and the mui-open-nomui test drive it that way), so the
 * host always counts as attached; only the Amiga shell needs a console window opened for it. */
static int host_attached(void) { return 1; }
static int host_open(char* err, size_t cap) {
    snprintf(err, cap, "no console can be opened on this system");
    return 0;
}
static void host_close(void) {}
static const sel_console_ops g_host = {host_attached, host_open, host_close};
const sel_console_ops* sel_platform_console_ops(void) { return &g_host; }
#endif

static const sel_console_ops* g_ops;
static unsigned g_opens;
static int g_atexit;

static const sel_console_ops* ops(void) { return g_ops != NULL ? g_ops : sel_platform_console_ops(); }

void sel_console_set_ops(const sel_console_ops* o) { g_ops = o; }
unsigned sel_console_opens(void) { return g_opens; }
void sel_console_release(void) { ops()->close(); }

int sel_console_ensure(char* err, size_t cap) {
    if (ops()->attached()) {
        return 1;
    }
    if (!ops()->open(err, cap)) {
        return 0;
    }
    g_opens++;
    if (!g_atexit) {
        g_atexit = 1;
        (void)atexit(sel_console_release);
    }
    return 1;
}
