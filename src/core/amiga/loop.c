#include "loop.h"

#include <exec/tasks.h>
#include <dos/dos.h>
#include <proto/exec.h>

static struct Task* g_main;
static LONG g_sig = -1;
static uint32_t g_ui;

int sel_loop_init(uint32_t ui_mask) {
    g_main = FindTask(NULL);
    g_sig = AllocSignal(-1);
    g_ui = ui_mask;
    return g_sig < 0;
}

void sel_loop_exit(void) {
    if (g_sig >= 0) {
        FreeSignal(g_sig);
        g_sig = -1;
    }
}

void sel_loop_notify(void) {
    if (g_sig >= 0) {
        Signal(g_main, 1UL << g_sig);
    }
}

uint32_t sel_loop_turn(sel_engine* e, sel_event_sink sink, void* ctx) {
    uint32_t got = Wait((1UL << g_sig) | g_ui | SIGBREAKF_CTRL_C);

    sel_engine_pump(e, sink, ctx);
    return got;
}
