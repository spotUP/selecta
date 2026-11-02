/* I915: the debug UI-wait guard exists under ONE condition (SEL_LIBRARY_UI_GUARD in db.h), which the library definition and
 * the engine's caller both use. An Amiga build is Debug-linked with the engine seeing no SEL_LIBRARY_NO_THREAD (CMake sets it
 * only on the library target); the guard must then be absent for both. This header-only test plays the engine's view of an Amiga
 * Debug build: AMIGA defined, NDEBUG not, no SEL_LIBRARY_NO_THREAD. */
#undef NDEBUG
#undef SEL_LIBRARY_NO_THREAD
#define AMIGA 1
#include <stdio.h>

#include "db.h"

#if defined(SEL_LIBRARY_UI_GUARD)
#error "I915: the UI-wait guard must not exist in an Amiga Debug build (no worker thread, worker.c defines nothing)"
#endif

int main(void) {
    printf("library_ui_guard_macro_ok\n");
    return 0;
}
