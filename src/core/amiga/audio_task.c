/* Audio task: waits for the CIA-B tick, renders one buffer, hands it to the
 * sink. Runs at the audio.priority pref (default 30, plan M2 D-M2-1). Integer
 * only (T54): the render itself lives in the engine. */
#include "audio.h"

#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <proto/exec.h>

#define STACK_BYTES 8192u
#define MAX_FRAMES 2048u

static struct Task* g_task;
static UBYTE* g_stack;
static struct Task g_tcb;
static sel_engine* g_engine;
static uint32_t g_hz;
static uint32_t g_frames;
static volatile uint32_t g_ticks;
static volatile int g_stop;
static sel_audio_sink_fn g_sink;
static void* g_sink_ctx;
static struct Task* g_parent;
static int16_t g_buf[MAX_FRAMES * 2];

static void audio_main(void) {
    LONG sig = AllocSignal(-1);
    ULONG mask = sig >= 0 ? 1UL << sig : 0;

    if (sel_audio_int_start(g_task, mask, g_hz) != 0 || mask == 0) {
        g_stop = 1;
    }
    Signal(g_parent, SIGF_SINGLE); /* setup done (ok or not) */
    while (!g_stop) {
        ULONG got = Wait(mask | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C) {
            break;
        }
        sel_engine_render(g_engine, g_buf, g_frames);
        if (g_sink != NULL) {
            g_sink(g_sink_ctx, g_buf, g_frames);
        }
        g_ticks++;
    }
    sel_audio_int_stop();
    if (sig >= 0) {
        FreeSignal(sig);
    }
    g_task = NULL;
    Signal(g_parent, SIGF_SINGLE);
}

int sel_audio_task_start(sel_engine* e, int priority, uint32_t hz, uint32_t frames) {
    union {
        void (*fn)(void);
        APTR pc;
    } entry;
    if (g_task != NULL || frames == 0 || frames > MAX_FRAMES || hz == 0) {
        return 1;
    }
    g_stack = AllocMem(STACK_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (g_stack == NULL) {
        return 1;
    }
    g_engine = e;
    g_hz = hz;
    g_frames = frames;
    g_stop = 0;
    g_parent = FindTask(NULL);
    g_tcb.tc_Node.ln_Type = NT_TASK;
    g_tcb.tc_Node.ln_Pri = (BYTE)priority;
    g_tcb.tc_Node.ln_Name = (char*)"selecta audio";
    g_tcb.tc_SPLower = g_stack;
    g_tcb.tc_SPUpper = g_stack + STACK_BYTES;
    g_tcb.tc_SPReg = g_tcb.tc_SPUpper;
    entry.fn = audio_main; /* ISO C has no function-to-object cast; the union is the portable one */
    SetSignal(0, SIGF_SINGLE);
    g_task = &g_tcb;
    if (AddTask(&g_tcb, entry.pc, NULL) == NULL) {
        g_task = NULL;
        FreeMem(g_stack, STACK_BYTES);
        return 1;
    }
    Wait(SIGF_SINGLE);
    if (g_stop) { /* the task failed its setup and is ending */
        Wait(SIGF_SINGLE);
        FreeMem(g_stack, STACK_BYTES);
        return 1;
    }
    return 0;
}

void sel_audio_task_stop(void) {
    if (g_task == NULL) {
        return;
    }
    g_stop = 1;
    Signal(g_task, SIGBREAKF_CTRL_C);
    Wait(SIGF_SINGLE);
    FreeMem(g_stack, STACK_BYTES);
}

void sel_audio_task_set_sink(sel_audio_sink_fn fn, void* ctx) {
    g_sink_ctx = ctx;
    g_sink = fn;
}

uint32_t sel_audio_task_ticks(void) {
    return g_ticks;
}
