/* Amiga test program for amiga-stack-canary (tests/amiga/run_stack_canary.cmake, row I376).
 * Runs its body on a deliberately small swapped stack (4 KiB). Usage: selecta-stack-canary-test ok|overflow.
 * ok: the body uses a little stack and prints canary_ok=1. overflow: the body fills a stack buffer that runs down
 * past the canary at the bottom of the swapped stack, so the exit check in sel_amiga_run_with_stack must print its
 * "stack overflow" error line and make the exit code 1. */
#include "amiga_start.h"
#include <stdio.h>
#include <string.h>

#define TEST_STACK_BYTES 4096UL
#define TEST_OVERFLOW_BYTES (TEST_STACK_BYTES - 64UL)

static void fill_stack_buffer(void) {
    volatile unsigned char buf[TEST_OVERFLOW_BYTES];
    unsigned long i;
    for (i = 0; i < sizeof buf; i++) {
        buf[i] = (unsigned char)i;
    }
}

static int app_main(int argc, char** argv) {
    if (argc != 2) {
        fputs("usage: selecta-stack-canary-test ok|overflow\n", stderr);
        return 2;
    }
    if (strcmp(argv[1], "overflow") == 0) {
        fill_stack_buffer();
    }
    printf("canary_ok=%d swaps=%u\n", sel_amiga_stack_canary_ok(), sel_amiga_stack_swaps());
    return 0;
}

int main(int argc, char** argv) {
    return sel_amiga_run_with_stack(app_main, argc, argv, TEST_STACK_BYTES);
}
