/* Amiga console for the text UI fallback (I671b): a CON: window made current for Input() and Output(). */
#include "console_fallback.h"

#include <dos/dos.h>
#include <proto/dos.h>

#include <stdio.h>

static BPTR g_con;
static BPTR g_old_in, g_old_out;

static int amiga_attached(void) {
    BPTR in = Input();
    BPTR out = Output();
    return in != 0 && out != 0 && IsInteractive(in) != 0 && IsInteractive(out) != 0;
}

static int amiga_open(char* err, size_t cap) {
    char why[96];
    LONG code;
    if (g_con != 0) {
        return 1;
    }
    g_con = Open((CONST_STRPTR) "CON:0/0/640/200/Selecta/AUTO/CLOSE", MODE_NEWFILE);
    if (g_con == 0) {
        code = IoErr();
        if (!Fault(code, (CONST_STRPTR) "", (STRPTR)why, sizeof why) || why[0] == '\0') {
            snprintf(why, sizeof why, "DOS error %ld", (long)code);
        }
        snprintf(err, cap, "the CON: console window failed (%s)", why[0] == ':' ? why + 2 : why);
        return 0;
    }
    g_old_in = SelectInput(g_con);
    g_old_out = SelectOutput(g_con);
    return 1;
}

static void amiga_close(void) {
    if (g_con == 0) {
        return;
    }
    (void)SelectInput(g_old_in);
    (void)SelectOutput(g_old_out);
    Close(g_con);
    g_con = 0;
}

static const sel_console_ops g_amiga = {amiga_attached, amiga_open, amiga_close};
const sel_console_ops* sel_platform_console_ops(void) { return &g_amiga; }
