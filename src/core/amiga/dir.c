/* Amiga directory iteration: dos.library Lock, Examine, ExNext. Declared in
 * src/library/dir.h. Paths arrive as UTF-8 and go to DOS as Latin-1; names come back as UTF-8 (I218d). The FileInfoBlock is allocated with AllocDosObject so it
 * is longword aligned as Examine requires. */
#include "../../library/dir.h"
#include "../../library/latin1.h"
#include "dos_name.h"

#include <dos/dos.h>
#include <proto/dos.h>
#include <string.h>

int sel_dir_each(const char* path, sel_dir_fn fn, void* ctx) {
    char dos[SEL_DOS_PATH_MAX];
    BPTR lock = sel_dos_path(dos, sizeof dos, path) ? Lock((CONST_STRPTR)dos, SHARED_LOCK) : 0;
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
                char name[2 * sizeof fib->fib_FileName + 1];

                (void)sel_latin1_to_utf8(name, sizeof name, (const char*)fib->fib_FileName,
                                         strlen((const char*)fib->fib_FileName));
                if (!fn(ctx, name, fib->fib_DirEntryType > 0)) {
                    break;
                }
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return ok;
}

int64_t sel_dir_mtime(const char* path) {
    char dos[SEL_DOS_PATH_MAX];
    BPTR lock = sel_dos_path(dos, sizeof dos, path) ? Lock((CONST_STRPTR)dos, SHARED_LOCK) : 0;
    struct FileInfoBlock* fib;
    int64_t t = -1;

    if (lock == 0) {
        return -1;
    }
    fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL) {
        if (Examine(lock, fib) && fib->fib_DirEntryType > 0) {
            t = ((int64_t)fib->fib_Date.ds_Days * 1440 + fib->fib_Date.ds_Minute) * 3000 + fib->fib_Date.ds_Tick;
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return t;
}

int sel_file_stat(const char* path, uint32_t* size, int64_t* mtime) {
    char dos[SEL_DOS_PATH_MAX];
    BPTR lock = sel_dos_path(dos, sizeof dos, path) ? Lock((CONST_STRPTR)dos, SHARED_LOCK) : 0;
    struct FileInfoBlock* fib;
    int ok = 0;

    if (lock == 0) {
        return 0;
    }
    fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL) {
        if (Examine(lock, fib) && fib->fib_DirEntryType < 0) {
            *size = (uint32_t)fib->fib_Size;
            *mtime = ((int64_t)fib->fib_Date.ds_Days * 1440 + fib->fib_Date.ds_Minute) * 3000 + fib->fib_Date.ds_Tick;
            ok = 1;
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return ok;
}

int sel_file_touch(const char* path) {
    struct DateStamp ds;
    char dos[SEL_DOS_PATH_MAX];

    if (!sel_dos_path(dos, sizeof dos, path)) {
        return 0;
    }
    DateStamp(&ds);
    return SetFileDate((CONST_STRPTR)dos, &ds) != 0;
}
