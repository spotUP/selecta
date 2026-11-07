/* MUI external class library skeleton (I685b, design 2026-11-06-mui-mcc-package.md section 3.3). Built on the libnix
 * startup object lib/up_libinit.c (Resident with RTF_AUTOINIT, LibOpen / LibClose / LibExpunge / LibExtFunc, first code
 * returns -1 so the file is not runnable) and sys-include/stabs.h (ADDTABL_*, ADDTABL_END). Link with -nostartfiles and
 * up_libinit.c FIRST so the Resident is the first thing in the hunk.
 *
 * Vectors: Open (-6), Close (-12), Expunge (-18), Reserved (-24) from lib/up_libinit.c, then MCC_Query (-30).
 * MCC_Query(0) = the MUI_CustomClass, 1 and 2 = NULL (prefs class and image live in a .mcp).
 * Expunge: up_libinit.c refuses while the open count is above 0 and then calls __UserLibCleanup, which deletes the class
 * and closes the libraries. */
#include <stdint.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <clib/muimaster_protos.h>
#include <proto/muimaster.h>

#include "libinit.h"
#include "spike_class.h"

#define LIB_NAME "UP-Spike.mcc"
#define LIB_VER UP_LIB_VERSION
#define LIB_REV 0
#define LIB_ID "UP-Spike.mcc 1.0 (06.11.2026) Selecta MCC spike"

const UWORD LibVersion = LIB_VER;
const UWORD LibRevision = LIB_REV;
const char LibName[] = LIB_NAME;
const char LibIdString[] = LIB_ID;

struct ExecBase* SysBase;
struct Library* MUIMasterBase;
struct Library* UtilityBase;
struct Library* GfxBase;
struct Library* IntuitionBase;

static struct MUI_CustomClass* g_class;

static void close_libs(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
    if (IntuitionBase != NULL) {
        CloseLibrary(IntuitionBase);
        IntuitionBase = NULL;
    }
    if (GfxBase != NULL) {
        CloseLibrary(GfxBase);
        GfxBase = NULL;
    }
    if (UtilityBase != NULL) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
    }
}

/* Returns 0 on success (up_libinit.c fails the library load on non-zero). */
LONG __stdargs __UserLibInit(struct Library* lib, REG(a4, APTR a4)) {
    (void)a4;
    SysBase = *(struct ExecBase**)4;
    UtilityBase = OpenLibrary((CONST_STRPTR) "utility.library", 36);
    GfxBase = OpenLibrary((CONST_STRPTR) "graphics.library", 36);
    IntuitionBase = OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (UtilityBase == NULL || GfxBase == NULL || IntuitionBase == NULL || MUIMasterBase == NULL) {
        close_libs();
        return 1;
    }
    g_class = MUI_CreateCustomClass(lib, (char*)MUIC_Area, NULL, spike_instance_size(), (APTR)(uintptr_t)spike_dispatch);
    if (g_class == NULL) {
        close_libs();
        return 1;
    }
    return 0;
}

VOID __stdargs __UserLibCleanup(REG(a4, APTR a4)) {
    (void)a4;
    close_libs();
}

/* First user vector (LVO -30). which: 0 class, 1 prefs class, 2 prefs image. */
ULONG MCC_Query(LONG which) {
    return which == 0 ? (ULONG)g_class : 0;
}

ADDTABL_1(MCC_Query, d0)
ADDTABL_END()
