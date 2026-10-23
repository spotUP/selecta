/* Play queue (design doc: queue). Owned by the engine; every byte is
 * allocated once at engine creation, so the queue never allocates while
 * songs play. Entries are ordered; ids start at 1 and are never reused.
 * Paths live in a text arena that is compacted in place on removal. */
#ifndef SELECTA_ENGINE_QUEUE_H
#define SELECTA_ENGINE_QUEUE_H

#include <selecta/engine.h> /* sel_queue_entry */
#include <selecta/host.h>
#include <stdint.h>

#define SEL_QUEUE_NONE 0xFFFFFFFFu

typedef struct sel_queue {
    sel_queue_entry* entries;
    char* text;
    uint32_t capacity;
    uint32_t text_bytes;
    uint32_t text_used;
    uint32_t count;
    uint32_t next_id;
    uint32_t version;
} sel_queue;

/* Defaults scaled from the machine's fast RAM in bytes (0: unknown). */
void sel_queue_default_sizes(uint32_t fast_ram, uint32_t* capacity, uint32_t* text_bytes);

/* Returns 0 when allocation fails. capacity and text_bytes must be non-zero. */
int sel_queue_init(sel_queue* q, const sel_host* host, uint32_t capacity, uint32_t text_bytes);
void sel_queue_free(sel_queue* q, const sel_host* host);

/* Inserts `path` before position `at` (SEL_QUEUE_NONE or count: at the end).
 * Returns 0 when the entry table or the text arena is full. On success
 * *id_out is the new id and the version is bumped. */
int sel_queue_insert(sel_queue* q, uint32_t at, const char* path, uint32_t* id_out);

/* Removes the entry with this id; returns its former position, or
 * SEL_QUEUE_NONE when there is no such id. Bumps the version. */
uint32_t sel_queue_remove(sel_queue* q, uint32_t id);

/* Drops every entry; ids keep counting. Bumps the version. */
void sel_queue_clear(sel_queue* q);

/* Moves entry `from_id` to the position `to_id` occupies now (the entries in
 * between shift by one). Swaps two entries. 0: an id is not in the queue.
 * Both bump the version. */
int sel_queue_move(sel_queue* q, uint32_t from_id, uint32_t to_id);
int sel_queue_swap(sel_queue* q, uint32_t a_id, uint32_t b_id);

/* Position of an id, or SEL_QUEUE_NONE. */
uint32_t sel_queue_index_of(const sel_queue* q, uint32_t id);
const char* sel_queue_path(const sel_queue* q, uint32_t index);

#endif /* SELECTA_ENGINE_QUEUE_H */
