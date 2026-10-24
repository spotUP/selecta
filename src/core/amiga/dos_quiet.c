/* Requester suppression and silent existence probe for AmigaDOS names (I214). See dos_quiet.h. */
#include "dos_quiet.h"

#include "dos_name.h"

#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

void* sel_amiga_dos_quiet_begin(void) {
    struct Process* me = (struct Process*)FindTask(NULL);
    void* saved;
    if (me == NULL || me->pr_Task.tc_Node.ln_Type != NT_PROCESS) {
        return (void*)-2L; /* not a DOS process: nothing to change, nothing to restore */
    }
    saved = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1L;
    return saved;
}

void sel_amiga_dos_quiet_end(void* saved) {
    struct Process* me = (struct Process*)FindTask(NULL);
    if (saved == (void*)-2L || me == NULL) {
        return;
    }
    me->pr_WindowPtr = saved;
}

int sel_amiga_name_exists(const char* name) {
    char dos[SEL_DOS_PATH_MAX];
    void* saved;
    BPTR lock;

    if (!sel_dos_path(dos, sizeof dos, name)) {
        return 0;
    }
    saved = sel_amiga_dos_quiet_begin();
    lock = Lock((CONST_STRPTR)dos, ACCESS_READ);
    sel_amiga_dos_quiet_end(saved);
    if (lock == 0) {
        return 0;
    }
    UnLock(lock);
    return 1;
}
