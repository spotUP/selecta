/* I684c: the shell's wait rule (apps/common/shell_pace.h), including the handover case (no open output). */
#include "shell_pace.h"

#include <stdio.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail = 1; } } while (0)

int main(void) {
    CHECK(sel_shell_wait_ms(0, 1, 0, 20) == 20, "an open output whose pump never blocks needs the UI wait");
    CHECK(sel_shell_wait_ms(0, 1, 1, 20) == 0, "an open output that paces keeps wait(0)");
    CHECK(sel_shell_wait_ms(0, 0, 1, 20) == 20, "handed over (no output): the UI wait paces");
    CHECK(sel_shell_wait_ms(0, 0, 0, 20) == 20, "handed over and unpaced");
    CHECK(sel_shell_wait_ms(1, 1, 1, 20) == 20, "a clockless output keeps the UI wait");
    return g_fail;
}
