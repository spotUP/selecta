#ifndef SELECTA_APPS_COMMON_STARTUP_ERROR_H
#define SELECTA_APPS_COMMON_STARTUP_ERROR_H

/* The one place a fatal start-up failure of the Selecta shell is reported (I563). The text goes to stderr; on the
 * Amiga it also opens an intuition.library requester when no console is attached (Workbench start, no Output(),
 * or Output() not interactive), because a program started from the Workbench has no stderr anyone can read.
 * Returns how many reports this process has made (the count is the test sentinel, printed under --stats). */
unsigned sel_startup_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __AMIGA__
/* Platform part (apps/common/amiga_startup_error.c): shows `text` in a requester when no console is attached. */
void sel_amiga_startup_requester(const char* text);
#endif

#endif
