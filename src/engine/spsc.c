#include "spsc.h"

#include "../core/pal.h"

#include <string.h>

/* Head/tail publications so far (sentinel spsc_publishes=). */
static uint32_t g_publishes;

uint32_t sel_spsc_publishes(void) {
    return g_publishes;
}

static uint32_t round_pow2(uint32_t n) {
    uint32_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

int sel_spsc_init(sel_spsc* q, const sel_host* host, uint32_t capacity) {
    q->capacity = round_pow2(capacity < 2 ? 2 : capacity);
    q->head = q->tail = 0;
    q->slots = host->mem_alloc(host, (size_t)q->capacity * sizeof(sel_message), SEL_MEM_FAST);
    return q->slots != NULL;
}

void sel_spsc_free(sel_spsc* q, const sel_host* host) {
    if (q->slots) {
        host->mem_free(host, q->slots);
        q->slots = NULL;
    }
}

uint32_t sel_spsc_count(const sel_spsc* q) {
    return q->tail - q->head;
}

int sel_spsc_push(sel_spsc* q, const sel_message* m) {
    if (q->tail - q->head >= q->capacity) {
        return 0;
    }
    q->slots[q->tail & (q->capacity - 1)] = *m;
    sel_pal_publish_begin(); /* only the index store is guarded; the slot copy above is not */
    q->tail++;
    g_publishes++;
    sel_pal_publish_end();
    return 1;
}

int sel_spsc_pop(sel_spsc* q, sel_message* out) {
    if (q->tail == q->head) {
        return 0;
    }
    *out = q->slots[q->head & (q->capacity - 1)];
    sel_pal_publish_begin();
    q->head++;
    g_publishes++;
    sel_pal_publish_end();
    return 1;
}
