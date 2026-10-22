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
} sel_spsc;

int sel_spsc_init(sel_spsc* q, const sel_host* host, uint32_t capacity);
void sel_spsc_free(sel_spsc* q, const sel_host* host);
int sel_spsc_push(sel_spsc* q, const sel_message* m); /* 0 when full */
int sel_spsc_pop(sel_spsc* q, sel_message* out);      /* 0 when empty */
uint32_t sel_spsc_count(const sel_spsc* q);

#endif
