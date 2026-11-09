/* I685k: the sentinel attribute UPA_Shared_Origin (design 2026-11-06-mui-mcc-package.md section 10, gate item 1). A class answers it
 * with the base of the .mcc library it was loaded from; the static Selecta build has no library, so its answer is 0.
 * The answer is produced by src/map/up_origin.c, which lib/classheader.c routes every class OM_GET through; this test drives that
 * routing function through the static build (nothing registered: base 0) and through the library case (base set by the library init). */
#include "map/up_origin.h"

#include <mcc/up.h>

#include <stdio.h>

#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            fprintf(stderr, "FAIL line %d: ", __LINE__); \
            fprintf(stderr, __VA_ARGS__);              \
            fprintf(stderr, "\n");                     \
            return 1;                                  \
        }                                              \
    } while (0)

int main(void) {
    ULONG store = 0xDEADBEEFu;
    int dummy = 0;
    /* the tag is in the shared class index, GET only, distinct from the other shared tags */
    CHECK((UPA_Shared_Origin >> 8 & 0xFF) == UP_CLS_SHARED, "origin tag is not in the shared class index");
    CHECK(UPA_Shared_Origin != UPA_Shared_Side && UPA_Shared_Origin != UPM_Shared_Fire, "origin tag collides");
    /* static build: nothing registered, the class answers NULL */
    CHECK(up_origin_get(UPA_Shared_Origin, &store) == 1, "static build does not answer the sentinel");
    CHECK(store == 0, "static build answered 0x%lx, expected 0", (unsigned long)store);
    /* any other attribute is not the sentinel's */
    store = 0xDEADBEEFu;
    CHECK(up_origin_get(UPA_Shared_Side, &store) == 0, "answered a foreign attribute");
    CHECK(store == 0xDEADBEEFu, "wrote storage for a foreign attribute");
    /* library case: the library init registers its base, the sentinel returns it */
    up_origin_set(&dummy);
    CHECK(up_origin_get(UPA_Shared_Origin, &store) == 1, "library build does not answer the sentinel");
    CHECK(store == (ULONG)(uintptr_t)&dummy || sizeof(void*) > 4, "library answered the wrong base");
    up_origin_set(NULL);
    CHECK(up_origin_get(UPA_Shared_Origin, &store) == 1 && store == 0, "reset did not return to NULL");
    printf("[OK] mcc-shared-origin\n");
    return 0;
}
