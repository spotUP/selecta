/* up-mcc.library (I685h, design 2026-11-06-mui-mcc-package.md section 8.4): the support library of the UP- MUI classes. Same libnix
 * skeleton as lib/libheader.c (lib/up_libinit.c first on the link line, stabs.h function table), but no class: the functions are
 * lib/support_lib.c, linked from sel_mcc_static, so the static build and the library share one source.
 * Vectors from -30 in the order of dist/sfd/up-mcc_lib.sfd. The store lives in the library's data, shared by every opener. */
#include <stdint.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <libraries/mui.h>
#include <proto/muimaster.h>

#include "libinit.h"
#include "map/up_store.h"

#define LIB_NAME "up-mcc.library"
#define LIB_VER UP_LIB_VERSION
#define LIB_REV 0
#define LIB_ID "$VER: up-mcc.library 1.0 (07.11.2026) UP- MUI class package support library"

const UWORD LibVersion = LIB_VER;
const UWORD LibRevision = LIB_REV;
const char LibName[] = LIB_NAME;
const char LibIdString[] = LIB_ID;

struct ExecBase* SysBase;
struct Library* UtilityBase;
struct Library* sel_mui_GfxBase;
struct Library* IntuitionBase;
struct Library* DOSBase;
struct Library* MUIMasterBase; /* mui_aa_draw.c calls MUI_ObtainPen / MUI_ReleasePen */

/* libnix's stdio start-up (pulled in by the store's fopen / snprintf) names exit(); a library never exits a process. Reached only
 * by a libnix fatal path, where the calling task stops instead of running on with a broken stdio. */
void exit(int code) {
    (void)code;
    Wait(0L);
    for (;;) {
    }
}

static void close_libs(void) {
    if (IntuitionBase != NULL) {
        CloseLibrary(IntuitionBase);
        IntuitionBase = NULL;
    }
    if (sel_mui_GfxBase != NULL) {
        CloseLibrary(sel_mui_GfxBase);
        sel_mui_GfxBase = NULL;
    }
    if (UtilityBase != NULL) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
    }
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
    if (DOSBase != NULL) {
        CloseLibrary(DOSBase);
        DOSBase = NULL;
    }
}

/* Returns 0 on success (up_libinit.c fails the library load on non-zero). */
LONG __stdargs __UserLibInit(struct Library* lib, REG(a4, APTR a4)) {
    (void)lib;
    (void)a4;
    SysBase = *(struct ExecBase**)4;
    DOSBase = OpenLibrary((CONST_STRPTR) "dos.library", 36);
    UtilityBase = OpenLibrary((CONST_STRPTR) "utility.library", 36);
    sel_mui_GfxBase = OpenLibrary((CONST_STRPTR) "graphics.library", 36);
    IntuitionBase = OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (MUIMasterBase == NULL || DOSBase == NULL || UtilityBase == NULL || sel_mui_GfxBase == NULL || IntuitionBase == NULL) {
        close_libs();
        return 1;
    }
    up_registry_init();
    return 0;
}

VOID __stdargs __UserLibCleanup(REG(a4, APTR a4)) {
    (void)a4;
    up_registry_free();
    close_libs();
}

/* The graphics base keeps the package rename (src/mui_gfxbase.h, I719): the variable the draw code calls through. */
/* Prototypes: the functions of lib/support_lib.c (mcc/up.h declares them). */
#include <mcc/up.h>

ADDTABL_1(UP_ThemeLoad, a0)
ADDTABL_0(UP_ThemeReload)
ADDTABL_2(UP_ThemeSetTokens, a0, d0)
ADDTABL_0(UP_ThemeTokens)
ADDTABL_1(UP_IconFind, a0)
ADDTABL_1(UP_SetFontFile, a0)
ADDTABL_4(UP_ThemeAttach, a0, a1, a2, a3)
ADDTABL_1(UP_ThemeDetach, a0)
ADDTABL_0(UP_ThemeObjects)
ADDTABL_END()
