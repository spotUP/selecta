#ifndef __TIMEMANAGER__
#define __TIMEMANAGER__
#include "../core/core.h"
typedef uint32_t ticks_t;
ticks_t time_ticks_per_second(void);
ticks_t time_ticks(void);
ticks_t time_ticks_per_second_hires(void);
ticks_t time_ticks_hires(void);
#endif
