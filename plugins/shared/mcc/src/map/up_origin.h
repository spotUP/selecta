#ifndef UP_ORIGIN_H
#define UP_ORIGIN_H

/* The sentinel attribute UPA_Shared_Origin (I685k, design 2026-11-06-mui-mcc-package.md section 10, gate item 1).
 * A .mcc library registers its own base in its library init (lib/classheader.c); the dispatcher wrapper of the library answers a
 * GET of UPA_Shared_Origin through up_origin_get, so every class answers it without a line in the class files. The static Selecta
 * build never registers a base and its classes are not wrapped. Pure code: built on every target. */
#include <mcc/up.h>

/* The base of the library the code runs in; NULL registers "not loaded from a library". */
void up_origin_set(const void* library_base);

/* attr == UPA_Shared_Origin: writes the registered base (0 when none) to *storage and returns 1. Any other attribute: returns 0
 * and leaves *storage alone. */
int up_origin_get(ULONG attr, ULONG* storage);

#endif
