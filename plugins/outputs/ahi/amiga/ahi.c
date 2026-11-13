/* ahi output (plan M2 phase 4, I124): CMD_WRITE requests on ahi.device, two in flight, the second linked behind the
 * first through ahir_Link so AHI plays them gapless (AHI double buffering). Mode: the unit-0 default (AHI_DEFAULT_ID
 * is what unit 0 opens with). Amiga only; OS headers stay in this directory (RULES.md rule 3). The toolchain ships
 * no AHI headers, so the request layout of devices/ahi.h (AHI 4) is written out like src/core/amiga/machine.c does. */
#include <dos/dostags.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "../common/ahi_out.h"

#define AHI_DEVICE_NAME "ahi.device"
#define AHI_VERSION 4u
#define AHI_UNIT_DEFAULT 0u
#define AHIST_S16S 3UL
#define AHI_UNITY 0x10000L /* Fixed 1.0 */
#define AHI_CENTRE 0x8000L /* Fixed 0.5 */

typedef struct ahi_request {
    struct IOStdReq std;
    UWORD version;
    UWORD pad1;
    ULONG priv[2];
    ULONG type;
    ULONG frequency;
    LONG volume;
    LONG position;
    struct ahi_request* link;
} ahi_request; /* 80 bytes */

typedef struct ahi_dev {
    struct MsgPort* port;
    ahi_request* req[2];
    int open;
    int pending[2];
    uint32_t rate;
} ahi_dev;

static void* hw_open(const sel_host* host, uint32_t rate, uint32_t frames) {
    ahi_dev* d;
    uint32_t i;
    (void)frames;
    d = (ahi_dev*)host->mem_alloc(host, sizeof *d, SEL_MEM_FAST);
    if (d == NULL) {
        return NULL;
    }
    d->port = CreateMsgPort();
    d->req[0] = d->port != NULL ? (ahi_request*)CreateIORequest(d->port, sizeof(ahi_request)) : NULL;
    d->req[1] = d->req[0] != NULL ? (ahi_request*)CreateIORequest(d->port, sizeof(ahi_request)) : NULL;
    d->open = 0;
    d->pending[0] = d->pending[1] = 0;
    d->rate = rate;
    if (d->req[1] != NULL) {
        d->req[0]->version = AHI_VERSION;
        d->open = OpenDevice((CONST_STRPTR)AHI_DEVICE_NAME, AHI_UNIT_DEFAULT, (struct IORequest*)d->req[0], 0) == 0;
    }
    if (!d->open) {
        for (i = 0; i < 2u; i++) {
            if (d->req[i] != NULL) {
                DeleteIORequest((struct IORequest*)d->req[i]);
            }
        }
        if (d->port != NULL) {
            DeleteMsgPort(d->port);
        }
        host->mem_free(host, d);
        return NULL;
    }
    /* The second request is a copy of the opened one (same device and unit). */
    d->req[1]->std = d->req[0]->std;
    d->req[1]->version = AHI_VERSION;
    return d;
}

static void hw_close(const sel_host* host, void* ctx) {
    ahi_dev* d = (ahi_dev*)ctx;
    CloseDevice((struct IORequest*)d->req[0]);
    DeleteIORequest((struct IORequest*)d->req[0]);
    DeleteIORequest((struct IORequest*)d->req[1]);
    DeleteMsgPort(d->port);
    host->mem_free(host, d);
}

static void hw_submit(void* ctx, uint32_t idx, const int16_t* pcm, uint32_t frames) {
    ahi_dev* d = (ahi_dev*)ctx;
    ahi_request* r = d->req[idx];
    r->std.io_Command = CMD_WRITE;
    r->std.io_Flags = 0;
    r->std.io_Data = (APTR)pcm;
    r->std.io_Length = frames * 4u;
    r->std.io_Offset = 0;
    r->type = AHIST_S16S;
    r->frequency = d->rate;
    r->volume = AHI_UNITY;
    r->position = AHI_CENTRE;
    r->link = d->pending[idx ^ 1u] ? d->req[idx ^ 1u] : NULL;
    SendIO((struct IORequest*)r);
    d->pending[idx] = 1;
}

static void hw_wait(void* ctx, uint32_t idx) {
    ahi_dev* d = (ahi_dev*)ctx;
    if (d->pending[idx]) {
        WaitIO((struct IORequest*)d->req[idx]);
        d->pending[idx] = 0;
    }
}

static int hw_finished(void* ctx, uint32_t idx) {
    ahi_dev* d = (ahi_dev*)ctx;
    return d->pending[idx] && CheckIO((struct IORequest*)d->req[idx]) != NULL;
}

/* Feeder process (I1340). A process, not a bare task: the mp3 decode inside the pull reads its file through DOS. */
typedef struct ahi_feeder {
    struct Task* parent;
    LONG sig; /* parent signal: ready, then done */
    void (*body)(void* arg, void* task);
    void* arg;
} ahi_feeder;

static ahi_feeder* volatile g_starting; /* handed to the new process; the parent waits until it is ready */

static void feeder_entry(void) {
    ahi_feeder* f = g_starting;
    f->body(f->arg, f);
    Forbid(); /* the parent may free the process memory only after this process is gone; Forbid holds until it ends */
    Signal(f->parent, 1UL << f->sig);
}

static void hw_task_ready(void* task) {
    ahi_feeder* f = (ahi_feeder*)task;
    Signal(f->parent, 1UL << f->sig);
}

static void* hw_task_start(const sel_host* host, void (*body)(void* arg, void* task), void* arg) {
    union {
        void (*fn)(void);
        APTR pc;
    } entry;
    struct Task* me = FindTask(NULL);
    struct Process* p;
    ahi_feeder* f = (ahi_feeder*)host->mem_alloc(host, sizeof *f, SEL_MEM_FAST);
    if (f == NULL) {
        return NULL;
    }
    f->parent = me;
    f->sig = AllocSignal(-1);
    f->body = body;
    f->arg = arg;
    if (f->sig < 0) {
        host->mem_free(host, f);
        return NULL;
    }
    SetSignal(0, 1UL << f->sig);
    g_starting = f;
    entry.fn = feeder_entry; /* ISO C has no function-to-object cast; the union is the portable one */
    p = CreateNewProcTags(NP_Entry, (ULONG)entry.pc, NP_Name, (ULONG) "Selecta audio feeder", NP_Priority,
                          (ULONG)(LONG)(me->tc_Node.ln_Pri + 1), NP_StackSize, 65536UL, TAG_DONE);
    if (p == NULL) {
        FreeSignal(f->sig);
        host->mem_free(host, f);
        return NULL;
    }
    Wait(1UL << f->sig); /* body opened the device (or failed to) */
    return f;
}

static void hw_task_join(const sel_host* host, void* task) {
    ahi_feeder* f = (ahi_feeder*)task;
    Wait(1UL << f->sig);
    FreeSignal(f->sig);
    host->mem_free(host, f);
}

static const sel_ahi_backend g_be = {"ahi",   "AHI",       hw_open,       hw_close,     hw_submit,
                                     hw_wait, hw_finished, hw_task_start, hw_task_ready, hw_task_join};
static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void* ahi_open(const sel_host* host, uint32_t rate, uint32_t channels) {
    return sel_ahi_out_open(&g_be, host, rate, channels);
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return sel_ahi_out_describe(&g_be, ahi_open, g_hooks, g_desc);
}
