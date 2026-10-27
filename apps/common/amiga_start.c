/* Shared Amiga start-up: gives the program a stack large enough for SQLite and the decoders (row I158b).
 * Why not the libnix `__stack` cookie: its swapstack.o (__stkinit) measures tc_SPUpper - tc_SPLower of the
 * running task and copies the live stack from tc_SPUpper down to sp. vamos never writes those two fields
 * for the main process, so the size reads 0 and the copy starts at address 0. This code reads the same
 * fields, treats 0 as "unknown" and swaps with exec StackSwap, which vamos implements.
 *
 * After StackSwap the A7-relative locals of the swapping function are no longer reachable, so every value
 * the code needs on both sides of a swap lives in statics (single threaded: main runs once). */
#include "amiga_start.h"
#include "stack_canary.h"

#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

/* A guard above the stack top, checked after the body: a store through A7 while it still points at the new stack
 * top (I271c) hits the guard instead of a neighbouring heap block, and is reported instead of corrupting it. */
#define SEL_STACK_GUARD 64u
#define SEL_STACK_GUARD_BYTE 0xA5u

static struct StackSwapStruct g_sss;
static int (*g_body)(int, char**);
static int g_argc;
static char** g_argv;
static int g_rc;
static unsigned g_swaps;
static unsigned long g_last_bytes;
static UBYTE* g_stack_low; /* the swapped stack while the body runs, NULL otherwise */

static void run_body(void) {
    g_rc = g_body(g_argc, g_argv);
}

/* Swap to g_sss, run run_body, swap back, as ONE asm block (I271c). Written as C, the compiler spills the
 * StackSwap argument (&g_sss) into an A7-relative temp: the second call then ran with A7 on the NEW stack and
 * stored &g_sss at stack top + 16, i.e. into whatever block sits right above the stack (the first AllocVec,
 * the library, whose queue pointer it overwrote; the atexit close then freed it). No compiler code runs
 * between the swaps now, and only absolute addresses are used. a6 and the scratch registers are declared
 * clobbered so the compiler saves and restores them on the old stack, outside the swapped region. */
static void __attribute__((noinline)) run_swapped(void) {
    __asm__ volatile("move.l %0,%%a0\n\t"
                     "move.l %1,%%a6\n\t"
                     "jsr -732(%%a6)\n\t"
                     "move.l %2,%%a1\n\t"
                     "jsr (%%a1)\n\t"
                     "move.l %0,%%a0\n\t"
                     "move.l %1,%%a6\n\t"
                     "jsr -732(%%a6)\n\t"
                     :
                     : "i"(&g_sss), "m"(SysBase), "i"(run_body)
                     : "d0", "d1", "a0", "a1", "a6", "cc", "memory");
}

int sel_amiga_run_with_stack(int (*body)(int, char**), int argc, char** argv, unsigned long bytes) {
    struct Task* me = FindTask(NULL);
    unsigned long have = (unsigned long)me->tc_SPUpper - (unsigned long)me->tc_SPLower;
    UBYTE* mem;
    unsigned long i;

    if (me->tc_SPLower != NULL && me->tc_SPUpper != NULL && have >= bytes) {
        return body(argc, argv);
    }
    mem = (UBYTE*)AllocMem(bytes + SEL_STACK_GUARD, MEMF_PUBLIC | MEMF_CLEAR);
    if (mem == NULL) {
        return body(argc, argv); /* no memory for a big stack: run on the one we have */
    }
    g_body = body;
    g_argc = argc;
    g_argv = argv;
    memset(mem + bytes, SEL_STACK_GUARD_BYTE, SEL_STACK_GUARD);
    sel_stack_canary_fill(mem, bytes); /* I376: canary at the low end, fill pattern above for the high-water mark */
    g_stack_low = mem;
    g_sss.stk_Lower = mem;
    g_sss.stk_Upper = (ULONG)(mem + bytes);
    g_sss.stk_Pointer = (APTR)(mem + bytes);
    g_swaps++;
    g_last_bytes = bytes;
    run_swapped();
    for (i = 0; i < SEL_STACK_GUARD; i++) {
        if (mem[bytes + i] != SEL_STACK_GUARD_BYTE) {
            fputs("amiga_start: stack top overwritten (a store through A7 after StackSwap)\n", stderr);
            g_rc = 1;
            break;
        }
    }
    if (!sel_stack_canary_ok(mem)) {
        fputs("amiga_start: stack overflow, canary at the stack bottom overwritten (stack too small)\n", stderr);
        g_rc = 1;
    }
#ifdef SEL_STACKMEASURE /* I523: the high-water mark of the swapped stack */
    fprintf(stderr, "amiga_start: stack used %lu of %lu bytes\n", bytes - sel_stack_untouched(mem, bytes), bytes);
#endif
    g_stack_low = NULL;
    FreeMem(mem, bytes + SEL_STACK_GUARD);
    return g_rc;
}

unsigned sel_amiga_stack_swaps(void) {
    return g_swaps;
}

unsigned long sel_amiga_stack_bytes(void) {
    return g_last_bytes;
}

int sel_amiga_stack_canary_ok(void) {
    return g_stack_low == NULL || sel_stack_canary_ok(g_stack_low);
}

unsigned long sel_amiga_stack_untouched(void) {
    return g_stack_low == NULL ? 0 : sel_stack_untouched(g_stack_low, g_last_bytes);
}
