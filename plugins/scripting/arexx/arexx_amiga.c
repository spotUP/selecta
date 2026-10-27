/* Amiga half of the ARexx port: exec message port plus rexxsyslib argstrings. Everything that decides what a
 * message means lives in arexx_core.c. */
#ifdef __amigaos__
#include "arexx.h"
#include <exec/memory.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
#include <rexx/storage.h>
#include <stdlib.h>
#include <string.h>

struct arexx_port {
    struct MsgPort* port;
    struct Library* rexx;
    char name[AREXX_NAME_MAX];
    arexx_counters counters;
};

struct RxsLib* RexxSysBase;

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
        const char* line = NULL;
        if (IsRexxMsg(m) && (m->rm_Action & RXCODEMASK) == RXCOMM) {
            line = (const char*)ARG0(m);
        }
        arexx_dispatch(line, (m->rm_Action & RXFF_RESULT) != 0, exec, exec_ctx, reply, sizeof reply, &r,
                       &p->counters);
        reply_with(p, m, &r);
        n++;
    }
    return n;
}

void arexx_close(arexx_port* p) {
    struct RexxMsg* m;
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
#endif
