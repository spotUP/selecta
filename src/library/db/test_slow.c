/* Test knob body (I910, I906d): kept out of db_internal.h because usleep is hidden by -std=c99 on glibc; this file alone is compiled
 * with _DEFAULT_SOURCE (src/library/CMakeLists.txt). Desktop only: the Amiga build makes the knob a no-op macro. */
#if !defined(__amigaos__) && !defined(AMIGA)
#include "db_internal.h"

#include <stdlib.h>
#include <unistd.h>

void sel_library_test_slow(const char* var) {
    const char* slow = getenv(var);

    if (slow != NULL && atoi(slow) > 0) usleep((useconds_t)atoi(slow) * 1000u);
}
#else
typedef int sel_library_test_slow_unused; /* ISO C forbids an empty translation unit */
#endif
