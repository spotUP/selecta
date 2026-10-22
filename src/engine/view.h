/* View model, regular style (design doc section 9b): one flat list over the
 * queue or the browse listing, a cursor and a window of visible rows. Pure
 * state over the queue and browse buffers; the engine owns commands and
 * events (cmd_VIEW_* in engine.c). The rows point into those buffers, so they
 * are valid until the next pump. The window rows are allocated once. */
#ifndef SELECTA_ENGINE_VIEW_H
#define SELECTA_ENGINE_VIEW_H

#include <selecta/engine.h>
#include <selecta/host.h>

#include "local.h"
#include "queue.h"

#define SEL_VIEW_WINDOW_MAX 256u
#define SEL_VIEW_WINDOW_DEFAULT 20u

/* The data the view lists; read-only here. */
typedef struct sel_view_src {
    const sel_queue* queue;
    const sel_browse* browse;
    uint32_t playing; /* queue id of the playing entry, 0 none */
} sel_view_src;

typedef struct sel_view_sig {
    uint32_t source, style, row_count, cursor, window_start, window_rows, queue_version, browse_generation, playing;
} sel_view_sig;

typedef struct sel_view {
    sel_view_state pub;
    sel_view_row* rows;
    uint32_t row_cap;
    uint32_t cursor_of[2], id_of[2]; /* the cursor each source was left at */
    uint32_t browse_seen;            /* browse generation the cursor belongs to */
    uint32_t land;                   /* cursor for the next browse listing */
    int active;                      /* a VIEW_ command ran: events are wanted */
    int force_changed;               /* emit VIEW_CHANGED at the next sync */
    sel_view_sig last;
} sel_view;

/* Returns 0 when allocation fails. */
int sel_view_init(sel_view* v, const sel_host* host, uint32_t row_cap);
void sel_view_free(sel_view* v, const sel_host* host);

/* "queue" or "browse". 0: unknown source. */
int sel_view_show(sel_view* v, const sel_view_src* s, const char* source);

/* "N" absolute, "+N" / "-N" relative, "top", "bottom"; clamped to the list.
 * 0: the text is not a cursor position. */
int sel_view_cursor(sel_view* v, const sel_view_src* s, const char* spec);

/* 1..row_cap rows. 0: out of range. */
int sel_view_window(sel_view* v, const sel_view_src* s, uint32_t rows);

/* Re-anchors the cursor (a queue row by id), fits the window and fills the
 * rows. Cheap enough to call after every command. */
void sel_view_refresh(sel_view* v, const sel_view_src* s);

/* Refreshes, then compares with the state at the previous call. Returns 1
 * when anything the front end shows changed; *structure is set when the
 * source, style or window height changed (VIEW_CHANGED, else VIEW_ROWS only). */
int sel_view_changed(sel_view* v, const sel_view_src* s, int* structure);

#endif /* SELECTA_ENGINE_VIEW_H */
