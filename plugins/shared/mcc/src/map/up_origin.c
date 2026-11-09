/* See up_origin.h. */
#include "up_origin.h"

static const void* g_base;

void up_origin_set(const void* library_base) {
    g_base = library_base;
}

int up_origin_get(ULONG attr, ULONG* storage) {
    if (attr != UPA_Shared_Origin) {
        return 0;
    }
    *storage = (ULONG)(uintptr_t)g_base;
    return 1;
}
