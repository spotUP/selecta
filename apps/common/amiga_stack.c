/* Stack request for the 68k executables. libnix reads the `__stack` cookie at startup and, when it is
 * larger than the stack the launcher gave (AmigaDOS shell and vamos default: 4 KiB), switches to a stack
 * of that size. SQLite open, schema migration and the scan call chain need more than 8 KiB (measured
 * under vamos: 8 KiB crashes, 16 KiB runs), and the decoders go deeper, so each binary states its need. */
unsigned long __stack = 65536UL;

/* Link libnix swapstack.o (the code that reads `__stack`); nothing else references it. */
extern void __stkinit(void);
void (*const sel_keep_stkinit)(void) = __stkinit;
