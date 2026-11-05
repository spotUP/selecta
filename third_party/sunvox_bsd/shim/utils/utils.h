#ifndef __UTILS_H__
#define __UTILS_H__
#include "../core/core.h"
void set_seed(unsigned long seed);
unsigned long pseudo_random(void);
/* The engine renders on one thread: the sampler's lock is a counter that only has to exist. */
struct sundog_mutex { volatile int locked; };
int sundog_mutex_init(sundog_mutex* mutex, int attr);
int sundog_mutex_destroy(sundog_mutex* mutex);
int sundog_mutex_lock(sundog_mutex* mutex);
int sundog_mutex_unlock(sundog_mutex* mutex);
#ifdef __cplusplus
static inline void int_to_string( int value, UTF8_CHAR *dest ) { sprintf( (char*)dest, "%d", value ); }
#endif
#endif
