#ifndef SELECTA_AMIGA_START_H
#define SELECTA_AMIGA_START_H

/* Shared Amiga start-up (row I158b). The 68k executables run on the stack their launcher gave: 4 KiB from the
 * AmigaDOS shell default or vamos. The library open (SQLite) needs 16 KiB, a scan 64 KiB, decoders more, so
 * every Amiga main runs its body through sel_amiga_run_with_stack. */

#define SEL_AMIGA_STACK_BYTES (128UL * 1024UL)

/* Runs body(argc, argv). When the current task's stack bounds are unset (0, as vamos leaves them for the main
 * process) or span fewer than `bytes`, allocates a stack of `bytes`, runs the body on it through exec
 * StackSwap, swaps back and frees it. Returns the body's result. */
int sel_amiga_run_with_stack(int (*body)(int, char**), int argc, char** argv, unsigned long bytes);

/* Sentinel: number of stack swaps done and the size of the last swapped stack (0 when none ran). */
unsigned sel_amiga_stack_swaps(void);
unsigned long sel_amiga_stack_bytes(void);

#endif
