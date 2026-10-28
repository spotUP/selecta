/* Amiga half of the ARexx port: exec message port plus rexxsyslib argstrings. Everything that decides what a
 * message means lives in arexx_core.c. */
#ifdef __amigaos__
#include "arexx.h"
#include <exec/memory.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
#include <rexx/storage.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>

struct arexx_port {
    struct MsgPort* port;
    struct Library* rexx;
    char name[AREXX_NAME_MAX];
    arexx_counters counters;
};

/* Initialised on purpose: a tentative definition is a common symbol, and ld then pulls libnix's autoopen member for
 * RexxSysBase, which opens rexxsyslib.library at startup and aborts the whole program (rc 20) when the library is
 * absent (vamos). A strong definition keeps the open in arexx_open, where a missing library only leaves ARexx off. */
struct RxsLib* RexxSysBase = NULL;

arexx_port* arexx_open(void) {
    arexx_port* p;
    unsigned n;
    RexxSysBase = (struct RxsLib*)OpenLibrary((CONST_STRPTR)"rexxsyslib.library", 0);
    if (RexxSysBase == NULL) {
        return NULL;
    }
    p = calloc(1, sizeof *p);
    if (p == NULL) {
        CloseLibrary((struct Library*)RexxSysBase);
        return NULL;
    }
    p->rexx = (struct Library*)RexxSysBase;
    p->port = CreateMsgPort();
    if (p->port == NULL) {
        CloseLibrary((struct Library*)RexxSysBase);
        free(p);
        return NULL;
    }
    for (n = 0; arexx_port_name(n, p->name, sizeof p->name); n++) {
        int added = 0;
        Forbid();
        if (FindPort((CONST_STRPTR)p->name) == NULL) {
            p->port->mp_Node.ln_Name = p->name;
            p->port->mp_Node.ln_Pri = 0;
            AddPort(p->port);
            added = 1;
        }
        Permit();
        if (added) {
            return p;
        }
    }
    DeleteMsgPort(p->port); /* every name SELECTA .. SELECTA.99 is taken: ARexx stays off */
    CloseLibrary((struct Library*)RexxSysBase);
    free(p);
    return NULL;
}

const char* arexx_name(const arexx_port* p) {
    return p->name;
}

const arexx_counters* arexx_stats(const arexx_port* p) {
    return &p->counters;
}

static void reply_with(arexx_port* p, struct RexxMsg* m, const arexx_result* r) {
    m->rm_Result1 = r->result1;
    m->rm_Result2 = 0;
    if (r->has_string && r->result1 == 0 && (m->rm_Action & RXFF_RESULT) != 0) {
        m->rm_Result2 = (LONG)CreateArgstring((CONST_STRPTR)r->text, (ULONG)strlen(r->text));
    }
    (void)p;
    ReplyMsg((struct Message*)m);
}

unsigned arexx_poll(arexx_port* p, arexx_exec_fn exec, void* exec_ctx) {
    static char reply[16384u];
    struct RexxMsg* m;
    unsigned n = 0;
    while (n < AREXX_MSGS_PER_TURN && (m = (struct RexxMsg*)GetMsg(p->port)) != NULL) {
        arexx_result r;
        /* No IsRexxMsg() gate: it rejects messages a sender builds without CreateRexxMsg (wasabid), and the action
         * code is what separates a command from anything else (I597). */
        const char* line = arexx_message_line((unsigned long)m->rm_Action, (const char*)ARG0(m));
        arexx_dispatch(line, (m->rm_Action & RXFF_RESULT) != 0, exec, exec_ctx, reply, sizeof reply, &r,
                       &p->counters);
        reply_with(p, m, &r);
        n++;
    }
    return n;
}

void arexx_close(arexx_port* p, arexx_exec_fn exec, void* exec_ctx, int batch_open) {
    static char reply[16384u];
    struct RexxMsg* m;
    (void)arexx_end_batch(batch_open, exec, exec_ctx, reply, sizeof reply, &p->counters);
    Forbid();
    RemPort(p->port);
    while ((m = (struct RexxMsg*)GetMsg(p->port)) != NULL) {
        arexx_result r;
        arexx_closing_result(&r, &p->counters);
        reply_with(p, m, &r);
    }
    Permit();
    DeleteMsgPort(p->port);
    CloseLibrary(p->rexx);
    free(p);
}

/* Song-change hook launcher (M4-AX-4-2): refuses early when the script file or RexxMast (port "REXX") is missing,
 * so the caller gets one log line; otherwise rx runs detached (SYS_Asynch) and the shell loop never waits for it. */
int arexx_hook_launch(void* ctx, const char* script, const char* cmdline) {
    BPTR lock;
    BPTR nil_in;
    BPTR nil_out;
    LONG rc;
    (void)ctx;
    lock = Lock((CONST_STRPTR)script, ACCESS_READ);
    if (lock == 0) {
        return 0;
    }
    UnLock(lock);
    Forbid();
    rc = FindPort((CONST_STRPTR)"REXX") != NULL;
    Permit();
    if (!rc) {
        return 0;
    }
    nil_in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    nil_out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    rc = SystemTags((CONST_STRPTR)cmdline, SYS_Input, nil_in, SYS_Output, nil_out, SYS_Asynch, TRUE, TAG_END);
    if (rc == -1) { /* SystemTags did not take the handles over */
        if (nil_in != 0) {
            Close(nil_in);
        }
        if (nil_out != 0) {
            Close(nil_out);
        }
        return 0;
    }
    return 1;
}
#endif
