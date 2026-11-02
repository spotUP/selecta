#ifndef SELECTA_STACK_CANARY_H
#define SELECTA_STACK_CANARY_H

/* Bottom-of-stack canary and high-water mark for the swapped Amiga stack (row I376). Pure functions on a byte
 * buffer so the host builds and tests them; apps/common/amiga_start.c applies them to the AllocMem'd stack.
 * The stack grows down, so an overflow runs into the LOWEST bytes first: the first SEL_STACK_CANARY bytes hold
 * a canary pattern, the rest of the stack a fill pattern whose surviving run from the bottom measures how much
 * stack was never used. */

#define SEL_STACK_CANARY 256u
/* The lowest longword belongs to exec (I974): Kickstart 3.1 exec 40.10 StackSwap stores the cookie 0xBAD1BAD3 at
 * the stk_Lower of the stack it LEAVES (ROM $F818DE: movea.l (a0),a1; move.l #$BAD1BAD3,(a1)), so the swap back
 * from our stack writes it at low[0..3]. The canary is checked above it; a real overflow comes from above and
 * breaks low[4..] before it can reach low[0..3]. */
#define SEL_STACK_EXEC_COOKIE 4u
#define SEL_STACK_CANARY_BYTE 0x5Au
#define SEL_STACK_FILL_BYTE 0xC3u

/* Fills low[0 .. bytes): canary at the bottom, fill pattern above. bytes must be at least SEL_STACK_CANARY. */
void sel_stack_canary_fill(unsigned char* low, unsigned long bytes);

/* 1 when the canary bytes above exec's cookie longword (low[SEL_STACK_EXEC_COOKIE .. SEL_STACK_CANARY)) still hold
 * the canary pattern, else 0. */
int sel_stack_canary_ok(const unsigned char* low);

/* Bytes from the bottom up that were never written: the canary plus the unbroken fill run above it. 0 when the
 * stack ran down through the whole fill into a broken canary. A canary broken while fill above it survives was
 * not reached by the stack, so the fill run still measures the use (I974: no false "used N of N"). */
unsigned long sel_stack_untouched(const unsigned char* low, unsigned long bytes);

#endif
