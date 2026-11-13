#ifndef SELECTA_APPS_COMMON_STARTUP_ERROR_H
#define SELECTA_APPS_COMMON_STARTUP_ERROR_H

/* The one place a fatal start-up failure of the Selecta shell is reported (I563). The text goes to stderr; on the
 * Amiga it also always opens an intuition.library requester (I1352): a Workbench start has no stderr anyone can read,
 * and a Shell start must not fail silently either.
 * Returns how many reports this process has made (the count is the test sentinel, printed under --stats). */
unsigned sel_startup_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __AMIGA__
/* Platform part (apps/common/amiga_startup_error.c): shows `text` in a requester (title Selecta, button Ok). */
void sel_amiga_startup_requester(const char* text);
#endif

#endif
