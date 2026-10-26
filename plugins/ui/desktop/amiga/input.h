/* The IDCMP to sel_gui_input translation both Amiga desktop backends (rtg, aga) share (design doc
 * thoughts/shared/plans/2026-10-26-aga-c2p-backend.md 2.6). A backend owns one desk_amiga_input, feeds it every
 * IntuiMessage of its window and takes the translated events with desk_amiga_input_next. */
#ifndef SELECTA_DESKTOP_AMIGA_INPUT_H
#define SELECTA_DESKTOP_AMIGA_INPUT_H

#include <selecta/gui/gui_input.h>
#include <stdint.h>

#include <devices/inputevent.h>

struct IntuiMessage;

#define DESK_AMIGA_INPUT_QUEUE 64

typedef struct desk_amiga_input {
    sel_gui_input_map_state map;
    sel_gui_input queue[DESK_AMIGA_INPUT_QUEUE];
    uint32_t q_head, q_count;
    uint32_t last_sec, last_micros;
    int32_t last_x, last_y;
    uint32_t clicks;
    struct InputEvent ie;
} desk_amiga_input;

/* Opens intuition.library and keymap.library for the translation (the libraries are opened here, not by libnix, see
 * input.c). 0 when one does not open. */
int desk_amiga_input_open(desk_amiga_input* in);
void desk_amiga_input_close(desk_amiga_input* in);
/* Translates one message of the window. 1 when it queued an event (or a press that waits for its release). */
int desk_amiga_input_handle(desk_amiga_input* in, struct IntuiMessage* m);
/* Takes the next queued event; 0 when the queue is empty. */
int desk_amiga_input_next(desk_amiga_input* in, sel_gui_input* ev);

#endif
