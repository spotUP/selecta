/* Creates the config directory (dos.library CreateDir) with DOS requesters off (I214). Declared in
 * <selecta/engine.h>. */
#include <selecta/engine.h>

#include "dos_name.h"
#include "dos_quiet.h"

#include <proto/dos.h>

int sel_config_mkdir(const char* dir) {
    char dos[SEL_DOS_PATH_MAX];
    void* saved;
    BPTR lock;

    if (!sel_dos_path(dos, sizeof dos, dir)) {
        return 0;
    }
    saved = sel_amiga_dos_quiet_begin(); /* no requester if the assign is missing */
    lock = CreateDir((CONST_STRPTR)dos);
    if (lock != 0) {
        UnLock(lock);
        sel_amiga_dos_quiet_end(saved);
        return 1;
    }
    /* Already there is fine. */
    lock = Lock((CONST_STRPTR)dos, ACCESS_READ);
    sel_amiga_dos_quiet_end(saved);
    if (lock != 0) {
        UnLock(lock);
        return 1;
    }
    return 0;
}
