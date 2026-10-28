/* Index ranges on the wire: "0-11,40,52-60" (multi-select design, section 3/4). One reader shared by the UI selection model
 * (src/uimodel/select.c) and the engine's SONGS_* verbs, so both sides agree on the grammar. Pure C99, no allocation. */
#ifndef SELECTA_RANGES_H
#define SELECTA_RANGES_H

#include <stdint.h>

/* Reads the next "a" or "a-b" run at *p (after an optional comma) and advances *p. Returns 1 with lo..hi (inclusive,
 * lo <= hi), 0 at the end of the text, -1 on a syntax error or lo > hi. */
static inline int sel_ranges_next(const char** p, uint32_t* lo, uint32_t* hi) {
    const char* s = *p;
    uint64_t a = 0, b;
    int digits = 0;
    if (*s == ',') s++;
    if (*s == '\0') {
        *p = s;
        return 0;
    }
    while (*s >= '0' && *s <= '9') {
        a = a * 10u + (uint64_t)(*s++ - '0');
        digits++;
        if (a > 0xFFFFFFFEu) return -1;
    }
    if (digits == 0) return -1;
    b = a;
    if (*s == '-') {
        s++;
        b = 0;
        digits = 0;
        while (*s >= '0' && *s <= '9') {
            b = b * 10u + (uint64_t)(*s++ - '0');
            digits++;
            if (b > 0xFFFFFFFEu) return -1;
        }
        if (digits == 0 || b < a) return -1;
    }
    if (*s != '\0' && *s != ',') return -1;
    *lo = (uint32_t)a;
    *hi = (uint32_t)b;
    *p = s;
    return 1;
}

#endif
