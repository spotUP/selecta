/* Amiga audio timing (plan M2 phase 2): a CIA-B timer interrupt that only
 * signals the audio task, and the audio task that runs the render loop.
 * Both objects are integer only (T54 no-float-interrupt). */
#ifndef SELECTA_CORE_AMIGA_AUDIO_H
#define SELECTA_CORE_AMIGA_AUDIO_H

#include <selecta/engine.h>

#include <stdint.h>

/* Called by the audio task after each render with the rendered stereo frames. */
typedef void (*sel_audio_sink_fn)(void* ctx, const int16_t* stereo, uint32_t frames);

/* Starts the CIA-B timer (hz interrupts per second) that signals `task_signal`
 * of the task owning `task`. 0 on success. */
int sel_audio_int_start(void* task, uint32_t signal_mask, uint32_t hz);
void sel_audio_int_stop(void);
/* Timer interrupts taken so far (rig sentinel cia_ints=). */
uint32_t sel_audio_int_count(void);

/* Creates the audio task at `priority` (the audio.priority pref): per tick it
 * renders `frames` stereo frames from `e` and hands them to the sink. 0 on success. */
int sel_audio_task_start(sel_engine* e, int priority, uint32_t hz, uint32_t frames);
void sel_audio_task_stop(void);
void sel_audio_task_set_sink(sel_audio_sink_fn fn, void* ctx);
/* Render passes done so far (rig sentinel audio_task_ticks=). */
uint32_t sel_audio_task_ticks(void);

#endif
