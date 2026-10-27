/* MUI availability probe and failure texts (plan 2026-10-25-amiga-mui.md MUI-P1-6, MUI-P1-4). */
#include <stddef.h>

#include "mui_map.h"

static mui_probe_fn g_probe;

void mui_set_probe(mui_probe_fn probe) {
    g_probe = probe;
}

int mui_present(void) {
    return g_probe != NULL && g_probe() != 0;
}

const char* mui_fail_text(mui_fail why) {
    return why == MUI_FAIL_NO_LIBRARY ? "Selecta: muimaster.library 19 (MUI 3.8) is not installed. Using the text UI."
                                      : "Selecta: the MUI window could not open. Using the text UI.";
}
