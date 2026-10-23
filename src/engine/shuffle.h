/* Shuffle (design doc: queue). xorshift32 and a Fisher-Yates pass, so the
 * same seed gives the same order on every host. No float, no allocation. */
#ifndef SELECTA_ENGINE_SHUFFLE_H
#define SELECTA_ENGINE_SHUFFLE_H

#include "queue.h"

/* One xorshift32 step. State 0 is a fixed point, so callers map it first. */
uint32_t sel_xorshift32(uint32_t x);

/* Maps the unusable seed 0 to a fixed non-zero constant. */
uint32_t sel_shuffle_seed(uint32_t seed);

/* Fisher-Yates over a plain id list with the same step rule as the queue
 * shuffle. Returns the final generator state. */
uint32_t sel_shuffle_ids(uint32_t* ids, uint32_t n, uint32_t seed);

/* Shuffles the entries from position `first` to the end with Fisher-Yates
 * (i from last down to first+1, j = step % span). Returns the final
 * generator state. Bumps the queue version. */
uint32_t sel_queue_shuffle(sel_queue* q, uint32_t first, uint32_t seed);

#endif /* SELECTA_ENGINE_SHUFFLE_H */
