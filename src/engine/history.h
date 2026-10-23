/* Queue history (design doc: queue). Every queue edit pushes the order it
 * replaces on an undo ring of SEL_HISTORY_DEPTH snapshots; UNDO and REDO walk
 * it. A snapshot is the list of entry ids only. Entries an edit removed move
 * to a "dead" table with their own path arena, so a snapshot can bring them
 * back. All memory is allocated once at engine creation. When the dead table
 * fills up the history is dropped (the edit stays, it is just not undoable). */
#ifndef SELECTA_ENGINE_HISTORY_H
#define SELECTA_ENGINE_HISTORY_H

#include "queue.h"

#define SEL_HISTORY_DEPTH 64u

typedef struct sel_history {
    uint32_t* ids; /* 2 * DEPTH snapshots of `capacity` ids: undo ring, then redo stack */
    uint32_t undo_len[SEL_HISTORY_DEPTH];
    uint32_t redo_len[SEL_HISTORY_DEPTH];
    uint32_t undo_head;  /* ring slot of the oldest undo snapshot */
    uint32_t undo_count;
    uint32_t redo_count;
    sel_queue_entry* dead;
    char* dead_text;
    sel_queue_entry* scratch;
    char* scratch_text;
    uint32_t dead_count;
    uint32_t dead_used;
    uint32_t capacity;
    uint32_t text_bytes;
} sel_history;

int sel_history_init(sel_history* h, const sel_host* host, uint32_t capacity, uint32_t text_bytes);
void sel_history_free(sel_history* h, const sel_host* host);

/* Call before an edit: remembers the current order, clears redo. */
void sel_history_record(sel_history* h, const sel_queue* q);

/* Drops the snapshot just recorded (the edit failed). */
void sel_history_forget(sel_history* h);

/* Call before removing the entry at `index` (SEL_QUEUE_NONE: every entry). */
void sel_history_bury(sel_history* h, const sel_queue* q, uint32_t index);

/* 1 when a step was applied (queue rewritten, version bumped by one). */
int sel_history_undo(sel_history* h, sel_queue* q);
int sel_history_redo(sel_history* h, sel_queue* q);

#endif /* SELECTA_ENGINE_HISTORY_H */
