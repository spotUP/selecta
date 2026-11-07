/* One generic UP-<Name>.mcc class library (I685i, design 2026-11-06-mui-mcc-package.md section 3.3). Same libnix skeleton as
 * lib/libheader.c (libinit.o first on the link line, stabs.h function table, -nostartfiles); compiled once per class with
 *   -DUP_CLASS_NAME="Button"  -DUP_CLASS_DESC=aabtn_class_desc
 * UP_CLASS_DESC names the aa_class_desc the class file exports (src/class/mui_aa_class.h): superclass, instance size, dispatcher
 * and the class pointer of the file. The class file is compiled into the same library with -DUP_MCC_LIBRARY, so its constructor
 * code uses the class created here. MUI_CreateCustomClass fills the mcc_* base fields of the MUI_CustomClass itself.
 * Vectors: Open (-6), Close (-12), Expunge (-18), Reserved (-24) from libinit.o, then MCC_Query (-30). Expunge: libinit.o
 * refuses while the open count is above 0, then calls __UserLibCleanup, which deletes the class and closes the libraries. */
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
#include "mui_aa_class.h"

#ifdef UP_PREFS_LIBRARY
/* I685j: the same skeleton builds UP-MCC.mcp. UP_CLASS_NAME is "MCC"; MCC_Query(1) returns the prefs class. */
#define LIB_NAME "UP-" UP_CLASS_NAME ".mcp"
extern const struct aa_class_desc up_prefs_class_desc; /* prefs/up_prefs.c */
#else
#define LIB_NAME "UP-" UP_CLASS_NAME ".mcc"
#endif
#define LIB_VER 1
#define LIB_REV 0
#define LIB_ID "$VER: " LIB_NAME " 1.0 (07.11.2026) UP- MUI class " UP_CLASS_NAME

const UWORD LibVersion = LIB_VER;
const UWORD LibRevision = LIB_REV;
const char LibName[] = LIB_NAME;
const char LibIdString[] = LIB_ID;

struct ExecBase* SysBase;
struct Library* UtilityBase;
struct Library* sel_mui_GfxBase; /* the package rename of the graphics base (src/mui_gfxbase.h, I719) */
struct Library* IntuitionBase;
struct Library* DOSBase;
struct Library* MUIMasterBase;

/* libnix's stdio start-up (pulled in by the theme store's fopen / snprintf) names exit(); a library never exits a process. */
void exit(int code) {
    (void)code;
    Wait(0L);
    for (;;) {
    }
}

static struct MUI_CustomClass* g_lib_class;

static void close_libs(void) {
    if (g_lib_class != NULL) {
        MUI_DeleteCustomClass(g_lib_class);
        g_lib_class = NULL;
        *UP_CLASS_DESC.slot = NULL;
    }
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
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
    if (DOSBase != NULL) {
        CloseLibrary(DOSBase);
        DOSBase = NULL;
    }
}

/* Returns 0 on success (libinit.o fails the library load on non-zero). */
LONG __stdargs __UserLibInit(struct Library* lib, REG(a4, APTR a4)) {
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
    g_lib_class = MUI_CreateCustomClass(lib, (char*)UP_CLASS_DESC.super, NULL, UP_CLASS_DESC.size, (APTR)(uintptr_t)UP_CLASS_DESC.dispatcher);
    if (g_lib_class == NULL) {
        close_libs();
        return 1;
    }
    *UP_CLASS_DESC.slot = g_lib_class;
    return 0;
}

VOID __stdargs __UserLibCleanup(REG(a4, APTR a4)) {
    (void)a4;
    close_libs();
}

/* First user vector (LVO -30). which: 0 class, 1 prefs class, 2 prefs image (both live in a .mcp, NULL here). */
ULONG MCC_Query(LONG which) {
#ifdef UP_PREFS_LIBRARY
    return which == 1 ? (ULONG)(uintptr_t)g_lib_class : 0;
#else
    return which == 0 ? (ULONG)(uintptr_t)g_lib_class : 0;
#endif
}

ADDTABL_1(MCC_Query, d0)
ADDTABL_END()
