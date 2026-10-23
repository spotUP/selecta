#include "shuffle.h"

uint32_t sel_xorshift32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

uint32_t sel_shuffle_seed(uint32_t seed) {
    return seed ? seed : 0x9E3779B9u;
}

uint32_t sel_queue_shuffle(sel_queue* q, uint32_t first, uint32_t seed) {
    uint32_t s = sel_shuffle_seed(seed);
    uint32_t i;
    if (first > q->count) {
        first = q->count;
    }
    for (i = q->count; i > first + 1u; i--) {
        uint32_t span = i - first; /* positions first .. i-1 */
        uint32_t j = first + sel_xorshift32(s) % span;
        sel_queue_entry t;
        s = sel_xorshift32(s);
        t = q->entries[i - 1u];
        q->entries[i - 1u] = q->entries[j];
        q->entries[j] = t;
    }
    q->version++;
    return s;
}
