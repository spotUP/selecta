/* Creates ENVARC:Selecta (dos.library CreateDir). Declared in <selecta/engine.h>. */
#include <selecta/engine.h>

#include <proto/dos.h>

int sel_config_mkdir(const char* dir) {
    BPTR lock = CreateDir((CONST_STRPTR)dir);
    if (lock != 0) {
        UnLock(lock);
        return 1;
    }
    /* Already there is fine. */
    lock = Lock((CONST_STRPTR)dir, ACCESS_READ);
    if (lock != 0) {
        UnLock(lock);
        return 1;
    }
    return 0;
}
