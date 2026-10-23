#include "p14_split.h"

void sel_p14_table_default(sel_p14_table* t) {
    uint32_t i;
    t->vol_hi = 64;
    t->vol_lo = 1;
    for (i = 0; i < 64; i++) {
        t->lo[i] = (int8_t)i;
    }
}

void sel_p14_split(const sel_p14_table* t, int16_t s, int8_t* hi, int8_t* lo) {
    int32_t s14 = (int32_t)s >> 2;
    *hi = (int8_t)(s14 >> 6);
    *lo = t->lo[s14 & 63];
}

int32_t sel_p14_merge(const sel_p14_table* t, int8_t hi, int8_t lo) {
    return (int32_t)hi * (int32_t)t->vol_hi + (int32_t)lo * (int32_t)t->vol_lo;
}

uint32_t sel_p14_max_error(const sel_p14_table* t) {
    int32_t s;
    uint32_t worst = 0;
    for (s = -32768; s <= 32767; s++) {
        int8_t hi, lo;
        int32_t d;
        sel_p14_split(t, (int16_t)s, &hi, &lo);
        d = sel_p14_merge(t, hi, lo) - (s >> 2);
        if (d < 0) {
            d = -d;
        }
        if ((uint32_t)d > worst) {
            worst = (uint32_t)d;
        }
    }
    return worst;
}
