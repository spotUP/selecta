/* Fixed-capacity single-producer single-consumer ring of sel_message.
 * Storage comes from the host allocator once, in sel_spsc_init. */
#ifndef SELECTA_SPSC_H
#define SELECTA_SPSC_H

#include <selecta/host.h>
#include <selecta/protocol.h>

typedef struct sel_spsc {
    sel_message* slots;
    uint32_t capacity; /* power of two */
    volatile uint32_t head; /* consumer */
    volatile uint32_t tail; /* producer */
    /* I1082b: string arguments of pushed messages are copied into a per-slot arena (arena_bytes each, 0: none) and
     * kept in place after pop: a popped message's strings stay valid until sel_spsc_release (the engine calls it at the end of
     * its pump), and the producer sees those slots as taken until then. */
    char* arena;
    uint32_t read; /* consumer cursor; with an arena the slots it passed stay owned (head) until sel_spsc_release */
    uint32_t arena_bytes;
} sel_spsc;

int sel_spsc_init(sel_spsc* q, const sel_host* host, uint32_t capacity);
void sel_spsc_free(sel_spsc* q, const sel_host* host);
/* Like sel_spsc_init, with a string arena of `arena_bytes` per slot: push copies every SEL_ARG_STR argument in (0 when
 * the strings do not fit), so the caller's buffers are free the moment push returns. */
int sel_spsc_init_arena(sel_spsc* q, const sel_host* host, uint32_t capacity, uint32_t arena_bytes);
int sel_spsc_push(sel_spsc* q, const sel_message* m); /* 0 when full (or the strings do not fit) */
int sel_spsc_pop(sel_spsc* q, sel_message* out);      /* 0 when empty */
void sel_spsc_release(sel_spsc* q); /* returns every popped slot to the producer (arena rings; a no-op otherwise) */
uint32_t sel_spsc_count(const sel_spsc* q);
/* Head/tail publications so far (each one ran inside sel_pal_publish_begin/end). */
uint32_t sel_spsc_publishes(void);

#endif
