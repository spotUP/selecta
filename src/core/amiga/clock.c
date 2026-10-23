/* Amiga clock: timer.device ReadEClock (E clock, 709379 Hz PAL, 715909 Hz NTSC), so the audio
 * task and the speed probe see sub-millisecond time. timer.device cannot be opened under vamos
 * (the OpenDevice call fails), so there the clock falls back to dos.library DateStamp, 50 ticks
 * per second. libnix's clock() opens timer.device at startup, which vamos cannot emulate, so the
 * Amiga build never links it. */
#include "host_internal.h"

#include <selecta/machine.h>

#include <devices/timer.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <exec/ports.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <stdlib.h>

struct Device* TimerBase; /* proto/timer.h inline stubs read this name */
#include <proto/timer.h>

static struct MsgPort* g_port;
static struct timerequest* g_req;
static uint32_t g_eclock_hz; /* 0 = not open (DateStamp fallback) */
static int g_tried;

static void clock_close(void) {
    if (g_req) {
        if (g_eclock_hz) {
            CloseDevice((struct IORequest*)g_req);
        }
        DeleteIORequest((struct IORequest*)g_req);
        g_req = NULL;
    }
    if (g_port) {
        DeleteMsgPort(g_port);
        g_port = NULL;
    }
    g_eclock_hz = 0;
}

/* timer.device is in ROM and always on exec's device list on a real Amiga. */
static int timer_device_resident(void) {
    struct Node* n;
    Forbid();
    n = FindName(&SysBase->DeviceList, (CONST_STRPTR)TIMERNAME);
    Permit();
    return n != NULL;
}

static void clock_open(void) {
    struct EClockVal ev;
    g_tried = 1;
    if (!timer_device_resident()) {
        return; /* vamos: OpenDevice on an absent device never returns */
    }
    g_port = CreateMsgPort();
    if (g_port) {
        g_req = (struct timerequest*)CreateIORequest(g_port, sizeof(struct timerequest));
    }
    if (g_req && OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest*)g_req, 0) == 0) {
        TimerBase = g_req->tr_node.io_Device;
        g_eclock_hz = (uint32_t)ReadEClock(&ev);
    }
    if (g_eclock_hz) {
        atexit(clock_close);
    } else {
        clock_close();
    }
}

const char* sel_clock_source(void) {
    if (!g_tried) {
        clock_open();
    }
    return g_eclock_hz ? "eclock" : "datestamp";
}

uint32_t sel_host_clock_ms(const sel_host* h) {
    (void)h;
    if (!g_tried) {
        clock_open();
    }
    if (g_eclock_hz) {
        struct EClockVal ev;
        unsigned long long ticks;
        ReadEClock(&ev);
        ticks = ((unsigned long long)ev.ev_hi << 32) | ev.ev_lo;
        return (uint32_t)(ticks * 1000ull / g_eclock_hz);
    }
    {
        struct DateStamp ds;
        DateStamp(&ds);
        return (uint32_t)ds.ds_Days * 86400000u + (uint32_t)ds.ds_Minute * 60000u +
               (uint32_t)ds.ds_Tick * 20u;
    }
}

/* DateStamp holds local time: minutes since midnight and 20 ms ticks. */
uint32_t sel_host_local_ms(const sel_host* h) {
    struct DateStamp ds;
    (void)h;
    DateStamp(&ds);
    return (uint32_t)ds.ds_Minute * 60000u + (uint32_t)ds.ds_Tick * 20u;
}
