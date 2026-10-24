/* Animation timeline of the desktop UI (plan 2.9, DUI-P3-5). One timeline per UI: a fixed array of at most 16 tweens,
 * values in Q16 (65536 = 1.0), time in milliseconds from the shell clock, easing tables of 64 entries. Integers only,
 * no allocation (plan 2.11). A tween is identified by a caller key; starting a key again restarts that tween. */
#ifndef SELECTA_GUI_ANIM_H
#define SELECTA_GUI_ANIM_H

#include <stdint.h>

#define SEL_GUI_TWEENS_MAX 16u
#define SEL_GUI_EASE_ENTRIES 64u

/* Durations (ms): hover and press are fast, view changes and slides are normal. */
enum { SEL_GUI_ANIM_FAST_MS = 100, SEL_GUI_ANIM_NORMAL_MS = 220 };

typedef enum { SEL_GUI_EASE_LINEAR = 0, SEL_GUI_EASE_OUT = 1, SEL_GUI_EASE_IN_OUT = 2, SEL_GUI_EASE_COUNT } sel_gui_ease;

typedef struct sel_gui_tween {
    uint32_t key;
    uint32_t start_ms, duration_ms;
    int32_t from, to, value; /* Q16 */
    uint32_t ease;
    uint32_t active;
} sel_gui_tween;

typedef struct sel_gui_timeline {
    sel_gui_tween tween[SEL_GUI_TWEENS_MAX];
    uint32_t table[SEL_GUI_EASE_COUNT][SEL_GUI_EASE_ENTRIES]; /* progress 0..65536 at i/63 of the duration */
    uint32_t now_ms;
    uint32_t started; /* tweens started over the timeline's life */
} sel_gui_timeline;

void sel_gui_timeline_init(sel_gui_timeline* t);
/* Starts (or restarts) the tween of `key` from `from` to `to` at the timeline's current time. Returns 0 when no slot
 * is free or the duration is 0: the caller then shows the end state at once. */
int sel_gui_tween_start(sel_gui_timeline* t, uint32_t key, int32_t from, int32_t to, uint32_t duration_ms, uint32_t ease);
/* Moves the clock to now_ms (never backwards) and updates every running tween; a finished tween frees its slot. */
void sel_gui_timeline_advance(sel_gui_timeline* t, uint32_t now_ms);
/* Q16 value of the running tween of `key`, `def` when none runs. */
int32_t sel_gui_tween_value(const sel_gui_timeline* t, uint32_t key, int32_t def);
uint32_t sel_gui_timeline_running(const sel_gui_timeline* t);
/* Ends every tween and frees the slots. */
void sel_gui_timeline_finish(sel_gui_timeline* t);
/* Eased progress 0..65536 for a linear position 0..65536 (table lookup, linear interpolation). */
uint32_t sel_gui_ease_at(const sel_gui_timeline* t, uint32_t ease, uint32_t pos_q16);

#endif
