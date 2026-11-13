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
    switch (why) {
    case MUI_FAIL_NO_LIBRARY:
        return "Selecta: muimaster.library 19 (MUI 3.8) is not installed. Install MUI 3.8 (muimaster.library in LIBS:) "
               "and start again. Using the text UI.";
    case MUI_FAIL_SCREEN_SMALL:
        return "Selecta: the MUI window could not open because the screen is too small. Use a larger screen mode "
               "(Screenmode preferences) or the text UI. Using the text UI.";
    case MUI_FAIL_NO_OBJECTS:
        return "Selecta: the MUI window could not open because MUI could not create its objects. Free some memory and "
               "check that the MUI classes and the UP-MCC classes are installed in MUI:Libs/mui. Using the text UI.";
    case MUI_FAIL_NO_GRAPHICS:
        return "Selecta: the MUI window could not open. graphics.library 37 (OS 2.04 or newer) is needed. "
               "Using the text UI.";
    case MUI_FAIL_NO_WINDOW:
    default:
        return "Selecta: the MUI window could not open. Free some memory, check the screen mode and that MUI 3.8 is "
               "installed, then start again. Using the text UI.";
    }
}
