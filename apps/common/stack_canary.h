#ifndef SELECTA_STACK_CANARY_H
#define SELECTA_STACK_CANARY_H

/* Bottom-of-stack canary and high-water mark for the swapped Amiga stack (row I376). Pure functions on a byte
 * buffer so the host builds and tests them; apps/common/amiga_start.c applies them to the AllocMem'd stack.
 * The stack grows down, so an overflow runs into the LOWEST bytes first: the first SEL_STACK_CANARY bytes hold
 * a canary pattern, the rest of the stack a fill pattern whose surviving run from the bottom measures how much
 * stack was never used. */

#define SEL_STACK_CANARY 256u
#define SEL_STACK_CANARY_BYTE 0x5Au
#define SEL_STACK_FILL_BYTE 0xC3u

/* Fills low[0 .. bytes): canary at the bottom, fill pattern above. bytes must be at least SEL_STACK_CANARY. */
void sel_stack_canary_fill(unsigned char* low, unsigned long bytes);

/* 1 when all SEL_STACK_CANARY bottom bytes still hold the canary pattern, else 0. */
int sel_stack_canary_ok(const unsigned char* low);

/* Bytes from the bottom up that were never written (canary plus the unbroken fill run above it), 0 when the
 * canary is broken. */
unsigned long sel_stack_untouched(const unsigned char* low, unsigned long bytes);

#endif
