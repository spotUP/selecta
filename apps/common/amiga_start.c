/* Shared Amiga start-up: gives the program a stack large enough for SQLite and the decoders (row I158b).
 * Why not the libnix `__stack` cookie: its swapstack.o (__stkinit) measures tc_SPUpper - tc_SPLower of the
 * running task and copies the live stack from tc_SPUpper down to sp. vamos never writes those two fields
 * for the main process, so the size reads 0 and the copy starts at address 0. This code reads the same
 * fields, treats 0 as "unknown" and swaps with exec StackSwap, which vamos implements.
 *
 * After StackSwap the A7-relative locals of the swapping function are no longer reachable, so every value
 * the code needs on both sides of a swap lives in statics (single threaded: main runs once). */
#include "amiga_start.h"

#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>

static struct StackSwapStruct g_sss;
static int (*g_body)(int, char**);
static int g_argc;
static char** g_argv;
static int g_rc;
static unsigned g_swaps;
static unsigned long g_last_bytes;

static void run_body(void) {
    g_rc = g_body(g_argc, g_argv);
}

int sel_amiga_run_with_stack(int (*body)(int, char**), int argc, char** argv, unsigned long bytes) {
    struct Task* me = FindTask(NULL);
    unsigned long have = (unsigned long)me->tc_SPUpper - (unsigned long)me->tc_SPLower;
    UBYTE* mem;

    if (me->tc_SPLower != NULL && me->tc_SPUpper != NULL && have >= bytes) {
        return body(argc, argv);
    }
    mem = (UBYTE*)AllocMem(bytes, MEMF_PUBLIC | MEMF_CLEAR);
    if (mem == NULL) {
        return body(argc, argv); /* no memory for a big stack: run on the one we have */
    }
    g_body = body;
    g_argc = argc;
    g_argv = argv;
    g_sss.stk_Lower = mem;
    g_sss.stk_Upper = (ULONG)(mem + bytes);
    g_sss.stk_Pointer = (APTR)(mem + bytes);
    g_swaps++;
    g_last_bytes = bytes;
    StackSwap(&g_sss);
    run_body();
    StackSwap(&g_sss);
    FreeMem(mem, bytes);
    return g_rc;
}

unsigned sel_amiga_stack_swaps(void) {
    return g_swaps;
}

unsigned long sel_amiga_stack_bytes(void) {
    return g_last_bytes;
}
