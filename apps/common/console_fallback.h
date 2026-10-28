#ifndef SELECTA_APPS_COMMON_CONSOLE_FALLBACK_H
#define SELECTA_APPS_COMMON_CONSOLE_FALLBACK_H

#include <stddef.h>

/* The text UI needs an interactive console. A program started with Run >NIL: or from a Workbench icon has none, so
 * the fallback from a UI that could not open (MUI) would be a dead end (I671b). sel_console_ensure makes sure a
 * console exists: it returns 1 at once when one is attached, otherwise opens one (an Amiga CON: window) and returns
 * 1, or returns 0 with the reason in err. sel_console_opens counts the consoles opened (the test sentinel). */
typedef struct sel_console_ops {
    int (*attached)(void);              /* 1 when stdin and stdout reach a person */
    int (*open)(char* err, size_t cap); /* open and select a console; 1 ok, 0 failed with the reason in err */
    void (*close)(void);                /* release what open made; no-op when nothing was opened */
} sel_console_ops;

/* The platform's real operations (console_fallback.c on the host, amiga_console.c on the Amiga). */
const sel_console_ops* sel_platform_console_ops(void);

/* Replaces the operations (tests); NULL restores the platform's. */
void sel_console_set_ops(const sel_console_ops* ops);

int sel_console_ensure(char* err, size_t cap);
unsigned sel_console_opens(void);
void sel_console_release(void);

#endif
