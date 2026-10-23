/* Creates the config directory (dos.library CreateDir) with DOS requesters off (I214). Declared in
 * <selecta/engine.h>. */
#include <selecta/engine.h>

#include "dos_quiet.h"

#include <proto/dos.h>

int sel_config_mkdir(const char* dir) {
    void* saved = sel_amiga_dos_quiet_begin(); /* no requester if the assign is missing */
    BPTR lock = CreateDir((CONST_STRPTR)dir);
    if (lock != 0) {
        UnLock(lock);
        sel_amiga_dos_quiet_end(saved);
        return 1;
    }
    /* Already there is fine. */
    lock = Lock((CONST_STRPTR)dir, ACCESS_READ);
    sel_amiga_dos_quiet_end(saved);
    if (lock != 0) {
        UnLock(lock);
        return 1;
    }
    return 0;
}
