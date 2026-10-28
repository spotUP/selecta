/* I671b: the text UI fallback opens a console when none is attached, and reports the real reason when that fails. */
#include "console_fallback.h"

#include <stdio.h>
#include <string.h>

static int g_attached, g_open_ok, g_open_calls, g_close_calls;

static int fake_attached(void) { return g_attached; }
static int fake_open(char* err, size_t cap) {
    g_open_calls++;
    if (!g_open_ok) {
        snprintf(err, cap, "the CON: console window failed (no memory)");
    }
    return g_open_ok;
}
static void fake_close(void) { g_close_calls++; }
static const sel_console_ops fake = {fake_attached, fake_open, fake_close};

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

int main(void) {
    char err[160] = "";
    sel_console_set_ops(&fake);

    g_attached = 1; /* a console exists: nothing is opened */
    CHECK(sel_console_ensure(err, sizeof err) == 1);
    CHECK(g_open_calls == 0 && sel_console_opens() == 0);

    g_attached = 0; /* Run >NIL: or Workbench: the console path is taken */
    g_open_ok = 1;
    CHECK(sel_console_ensure(err, sizeof err) == 1);
    CHECK(g_open_calls == 1 && sel_console_opens() == 1);

    g_open_ok = 0; /* the console fails too: 0, with the real reason */
    CHECK(sel_console_ensure(err, sizeof err) == 0);
    CHECK(g_open_calls == 2 && sel_console_opens() == 1);
    CHECK(strstr(err, "CON: console window failed") != NULL);

    sel_console_set_ops(NULL);
    printf("console_fallback_unit_ok\n");
    return 0;
}
