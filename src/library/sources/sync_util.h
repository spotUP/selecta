/* Helpers shared by the source syncs (modland_sync.c, hvsc_sync.c, hippo_sync.c). */
#ifndef SELECTA_LIBRARY_SOURCES_SYNC_UTIL_H
#define SELECTA_LIBRARY_SOURCES_SYNC_UTIL_H

#include <string.h>

/* Strips trailing line ends and spaces in place. */
static inline void sel_sync_chomp(char* s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
        s[--n] = '\0';
    }
}

#endif
