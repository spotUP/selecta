/* Amiga directory iteration: dos.library Lock, Examine, ExNext. Declared in
 * src/library/dir.h. The FileInfoBlock is allocated with AllocDosObject so it
 * is longword aligned as Examine requires. */
#include "../../library/dir.h"

#include <dos/dos.h>
#include <proto/dos.h>

int sel_dir_each(const char* path, sel_dir_fn fn, void* ctx) {
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock* fib;
    int ok = 0;

    if (lock == 0) {
        return 0;
    }
    fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL) {
        if (Examine(lock, fib) && fib->fib_DirEntryType > 0) {
            ok = 1;
            while (ExNext(lock, fib)) {
                if (!fn(ctx, (const char*)fib->fib_FileName, fib->fib_DirEntryType > 0)) {
                    break;
                }
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return ok;
}
