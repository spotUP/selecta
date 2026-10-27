/* Shared fatal start-up report (I563). See startup_error.h. */
#include "startup_error.h"

#include <stdarg.h>
#include <stdio.h>

static unsigned g_reports;

unsigned sel_startup_error(const char* fmt, ...) {
    char text[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    fprintf(stderr, "selecta: %s\n", text);
    fflush(stderr);
#ifdef __AMIGA__
    sel_amiga_startup_requester(text);
#endif
    return ++g_reports;
}
