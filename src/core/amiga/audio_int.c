/* CIA-B timer interrupt: counts the tick and signals the audio task. Nothing
 * else runs at interrupt level. Integer only (T54). cia.resource gives the
 * vector; timer A (bit 0) is tried first, timer B (bit 1) second. */
#include "audio.h"

#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <hardware/cia.h>
#define CIA_BASE_NAME CiaBase
#include <proto/cia.h>
#include <proto/exec.h>
#include <resources/cia.h>

#define CIAB_BASE ((volatile struct CIA*)0xBFD000)

static struct Library* CiaBase;
#define g_cia CiaBase
static struct Interrupt g_irq;
static volatile uint32_t g_count;
static struct Task* g_task;
static uint32_t g_mask;
static int g_bit = -1;

/* a1 carries the interrupt's is_Data (exec calls servers with it). */
static LONG tick(register void* data __asm("a1")) {
    (void)data;
    g_count++;
    Signal(g_task, g_mask);
    return 0;
}

int sel_audio_int_start(void* task, uint32_t signal_mask, uint32_t hz) {
    uint32_t period;
    int bit;

    if (hz == 0 || g_bit >= 0) {
        return 1;
    }
    g_cia = OpenResource((CONST_STRPTR)CIABNAME);
    if (g_cia == NULL) {
        return 1;
    }
    g_task = (struct Task*)task;
    g_mask = signal_mask;
    g_irq.is_Node.ln_Type = NT_INTERRUPT;
    g_irq.is_Node.ln_Name = (char*)"selecta audio";
    g_irq.is_Data = NULL;
    g_irq.is_Code = (void (*)(void))tick;
    for (bit = 0; bit < 2; bit++) {
        if (AddICRVector(g_cia, bit, &g_irq) == NULL) {
            break;
        }
    }
    if (bit == 2) {
        return 1;
    }
    g_bit = bit;
    period = ((struct ExecBase*)SysBase)->ex_EClockFrequency / hz;
    if (g_bit == 0) {
        CIAB_BASE->ciacra = 0;
        CIAB_BASE->ciatalo = (UBYTE)period;
        CIAB_BASE->ciatahi = (UBYTE)(period >> 8);
        CIAB_BASE->ciacra = 1; /* continuous, start */
    } else {
        CIAB_BASE->ciacrb = 0;
        CIAB_BASE->ciatblo = (UBYTE)period;
        CIAB_BASE->ciatbhi = (UBYTE)(period >> 8);
        CIAB_BASE->ciacrb = 1;
    }
    AbleICR(g_cia, 0x80 | (1 << g_bit));
    return 0;
}

void sel_audio_int_stop(void) {
    if (g_bit < 0) {
        return;
    }
    AbleICR(g_cia, 1 << g_bit);
    if (g_bit == 0) {
        CIAB_BASE->ciacra = 0;
    } else {
        CIAB_BASE->ciacrb = 0;
    }
    RemICRVector(g_cia, g_bit, &g_irq);
    g_bit = -1;
}

uint32_t sel_audio_int_count(void) {
    return g_count;
}
