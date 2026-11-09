/* I685h: compile check of the sfdc-generated client headers of up-mcc.library: every vector is reachable through the inline macros. */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/up_mcc.h>

struct Library* UPMCCBase;

LONG up_mcc_header_check(void) {
    ULONG rgb[1] = {0};
    LONG r = UP_ThemeLoad((STRPTR) "ENV:Selecta/theme");
    r += UP_ThemeReload();
    r += UP_ThemeSetTokens(rgb, 1);
    r += UP_ThemeTokens() != NULL;
    r += UP_IconFind((STRPTR) "play");
    r += UP_SetFontFile((STRPTR) "FONTS:Inter-Regular.ttf");
    return r;
}
