/* UP-Demo (I685k, design 2026-11-06-mui-mcc-package.md section 10 "Final gate", item 1; section 9 Demo).
 * A plain MUI 3.8 program built against the release headers only (include/mcc/up.h, up_lib.h, the SFD-generated headers): no Selecta
 * include path, no link against sel_mcc_static (ctest mcc-demo-headers-only). It opens each of the 11 shipped classes BY NAME,
 * MUI_NewObject("UP-<Name>.mcc"), which MUI loads from MUI:Libs/mui/, puts one instance of each into one window, and prints
 *   [OK] <class> from MUI:Libs/mui (origin 0x<hex>)       the class was created
 *   [ERROR] <class> ...                                   it was not (not installed, or no origin)
 * Exit code 0 when all 11 open, 20 otherwise. The origin is the sentinel UPA_Shared_Origin: the base of the .mcc library the object's
 * class came from. A NULL origin counts as an error, because it means the class code is not coming from a library.
 * Backend: the demo uses whatever the classes pick. Without ttengine they fall back to PLAIN text; the success criterion is the same
 * (gate item 3), so there is no switch here.
 * Gate item 2 (live theme reload from UP-MCC.mcp, row I685n): the token store and the retheme list live in up-mcc.library, so a
 * Save in the prefs recolours this window. Run: UP-Demo [QUIT | HOLD] (QUIT closes the window at once, HOLD keeps it open until
 * the close gadget, so the prefs can be opened and saved meanwhile). */
#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <dos/dos.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <mcc/up.h>

struct Library* MUIMasterBase;

#define CLASS_COUNT 11

static const char* const g_names[CLASS_COUNT] = {"Button", "Text", "Panel", "Tabs", "Balance", "Slider",
                                                 "Progress", "Cycle", "Field", "Scroller", "List"};
static const char* const g_cycle_entries[] = {"First", "Second", "Third", NULL};
static const char* const g_tab_captions[] = {"Songs", "Albums", NULL};

/* One instance of class `index` with the tags that class needs to be valid on its own; every class is created by file name. */
static Object* make(int index) {
    char path[32];
    Object* spacer = MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE);
    snprintf(path, sizeof path, "UP-%s.mcc", g_names[index]);
    switch (index) {
    case 0:
        return MUI_NewObject(path, UPA_Button_Caption, (ULONG) "Play", TAG_DONE);
    case 1:
        return MUI_NewObject(path, UPA_Text_Contents, (ULONG) "UP-Text from a library", TAG_DONE);
    case 2:
        return MUI_NewObject(path, MUIA_Group_Child, (ULONG)spacer, TAG_DONE);
    case 3:
        return MUI_NewObject(path, UPA_Tabs_Captions, (ULONG)g_tab_captions, MUIA_Group_Child, (ULONG)spacer, TAG_DONE);
    case 4:
        return MUI_NewObject(path, TAG_DONE);
    case 5:
        return MUI_NewObject(path, MUIA_Numeric_Min, 0UL, MUIA_Numeric_Max, 100UL, MUIA_Numeric_Value, 40UL, TAG_DONE);
    case 6:
        return MUI_NewObject(path, UPA_Progress_Value, 30UL, UPA_Progress_Max, 100UL, TAG_DONE);
    case 7:
        return MUI_NewObject(path, UPA_Cycle_Entries, (ULONG)g_cycle_entries, TAG_DONE);
    case 8:
        return MUI_NewObject(path, MUIA_Group_Child, (ULONG)spacer, TAG_DONE);
    case 9:
        return MUI_NewObject(path, UPA_Scroller_Entries, 100UL, UPA_Scroller_Visible, 10UL, TAG_DONE);
    default:
        return MUI_NewObject(path, UPA_List_Count, 20UL, TAG_DONE);
    }
}

int main(int argc, char** argv) {
    Object* obj[CLASS_COUNT];
    Object *app = NULL, *win, *root;
    struct TagItem kids[CLASS_COUNT + 4];
    ULONG sigs = 0, is_open = 0;
    int quit_now = argc > 1 && argv[1] != NULL && strcmp(argv[1], "QUIT") == 0;
    int hold = argc > 1 && argv[1] != NULL && strcmp(argv[1], "HOLD") == 0;
    int i, ok = 0, ticks = 0, k = 0;

    memset(obj, 0, sizeof obj);
    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (MUIMasterBase == NULL) {
        printf("[ERROR] muimaster.library 19\n");
        return 20;
    }
    for (i = 0; i < CLASS_COUNT; i++) {
        ULONG origin = 0;
        obj[i] = make(i);
        if (obj[i] == NULL) {
            printf("[ERROR] %s (MUI_NewObject UP-%s.mcc returned NULL)\n", g_names[i], g_names[i]);
            continue;
        }
        GetAttr(UPA_Shared_Origin, obj[i], &origin);
        if (origin == 0) {
            printf("[ERROR] %s (no origin: the class code is not from a library)\n", g_names[i]);
            continue;
        }
        printf("[OK] %s from MUI:Libs/mui (origin 0x%08lx)\n", g_names[i], (unsigned long)origin);
        ok++;
    }
    if (ok == CLASS_COUNT) {
        for (i = 0; i < CLASS_COUNT; i++) {
            kids[k].ti_Tag = MUIA_Group_Child;
            kids[k++].ti_Data = (ULONG)obj[i];
        }
        /* Two columns, so the 11 classes fit a readable window instead of one narrow tall strip. */
        kids[k].ti_Tag = MUIA_Group_Columns;
        kids[k++].ti_Data = 2;
        kids[k].ti_Tag = TAG_DONE;
        kids[k].ti_Data = 0;
        root = MUI_NewObjectA((char*)"UP-Panel.mcc", kids);
        win = root == NULL ? NULL
                           : MUI_NewObject((char*)MUIC_Window, MUIA_Window_Title, (ULONG) "UP-Demo", MUIA_Window_ID, 0x55504431, MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(40),
                                           MUIA_Window_Height, (ULONG)MUIV_Window_Height_Screen(40),
                                           MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
        if (win != NULL) {
            app = MUI_NewObject((char*)MUIC_Application, MUIA_Application_Title, (ULONG) "UP-Demo", MUIA_Application_Base,
                                (ULONG) "UPDEMO", MUIA_Application_Window, (ULONG)win, TAG_DONE);
        }
        if (app == NULL) {
            printf("[ERROR] window or application could not be created\n");
            ok = 0;
            if (win != NULL) {
                MUI_DisposeObject(win);
            } else if (root != NULL) {
                MUI_DisposeObject(root);
            }
        } else {
            DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2UL, MUIM_Application_ReturnID,
                     (ULONG)MUIV_Application_ReturnID_Quit);
            SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
            GetAttr(MUIA_Window_Open, win, &is_open);
            if (!is_open) {
                printf("[ERROR] window did not open\n");
                ok = 0;
            }
            while (!quit_now && (hold || ticks < 30) && is_open) { /* about 3 s, or until the close gadget (HOLD: only that) */
                ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
                if (id == (ULONG)MUIV_Application_ReturnID_Quit) {
                    break;
                }
                Delay(5);
                ticks++;
            }
            MUI_DisposeObject(app); /* the application owns window, group and the eleven objects */
        }
    } else {
        for (i = 0; i < CLASS_COUNT; i++) {
            if (obj[i] != NULL) {
                MUI_DisposeObject(obj[i]);
            }
        }
    }
    CloseLibrary(MUIMasterBase);
    return ok == CLASS_COUNT ? 0 : 20;
}
