/* Amiga main loop (plan M2 phase 2, I114): one Wait() on the engine, UI and
 * break signals, then sel_engine_pump. */
#ifndef SELECTA_CORE_AMIGA_LOOP_H
#define SELECTA_CORE_AMIGA_LOOP_H

#include <selecta/engine.h>

#include <stdint.h>

/* Allocates the engine signal; `ui_mask` is the front end's own signal set (window port, input). 0 on success. */
int sel_loop_init(uint32_t ui_mask);
void sel_loop_exit(void);
/* Wakes the loop from another task or an interrupt (engine work is waiting). */
void sel_loop_notify(void);
/* One turn: Wait(engine | ui | CTRL-C), then sel_engine_pump into `sink`.
 * Returns the signals that woke it; the caller handles its own `ui_mask` bits
 * and stops on SIGBREAKF_CTRL_C. */
uint32_t sel_loop_turn(sel_engine* e, sel_event_sink sink, void* ctx);

#endif
