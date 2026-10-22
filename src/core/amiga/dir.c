/* Amiga directory iteration (Examine/ExNext) arrives with the Amiga text UI
 * in M2. Until then every listing fails, so LIBRARY_BROWSE answers
 * ERROR browse_failed on the 68k build. */
#include "../../library/dir.h"

int sel_dir_each(const char* path, sel_dir_fn fn, void* ctx) {
    (void)path;
    (void)fn;
    (void)ctx;
    return 0;
}
