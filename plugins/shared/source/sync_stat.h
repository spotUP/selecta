/* Counter list helper shared by the source plugins and the library (I517). */
#ifndef SELECTA_SHARED_SOURCE_SYNC_STAT_H
#define SELECTA_SHARED_SOURCE_SYNC_STAT_H

#include <selecta/tags.h>

#include <stdint.h>

/* Appends one counter to a stats list that starts with SEL_TAG_END at [0]. A list that is full drops the counter. */
static inline void sel_sync_stat(sel_tag* stats, uint32_t cap, uint32_t tag, uintptr_t value) {
    uint32_t n = 0;

    while (stats[n].tag != SEL_TAG_END) {
        n++;
    }
    if (n + 2u > cap) {
        return;
    }
    stats[n].tag = tag;
    stats[n].value = value;
    stats[n + 1u].tag = SEL_TAG_END;
    stats[n + 1u].value = 0;
}

#endif
