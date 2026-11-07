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

/* I685w: the List shows 20 rows of text; without a row hook it had no text to draw. */
static struct Hook g_row_hook;
static ULONG demo_row(struct Hook* hook, Object* obj, UP_ListCell* cell) {
    static char text[32];
    (void)hook;
    (void)obj;
    snprintf(text, sizeof text, "Track %ld", (long)cell->row + 1);
    return (ULONG)(uintptr_t)text;
}

/* One instance of class `index` with the tags that class needs to be valid on its own; every class is created by file name. */
static Object* make(int index) {
    char path[32];
    /* I685w: a stock Rectangle fills with MUI's snapped background pen; the filler is an UP-Text that erases in the panel token */
    Object* spacer = MUI_NewObject((char*)"UP-Text.mcc", UPA_Text_Contents, (ULONG) " ", TAG_DONE);
    snprintf(path, sizeof path, "UP-%s.mcc", g_names[index]);
    switch (index) {
    case 0:
        return MUI_NewObject(path, UPA_Button_Caption, (ULONG) "Play", TAG_DONE);
    case 1:
        return MUI_NewObject(path, UPA_Text_Contents, (ULONG) "UP-Text from a library", TAG_DONE);
    case 2:
        return MUI_NewObject(path, UPA_Panel_Role, (ULONG)UPV_Panel_Role_Raised, MUIA_Group_Child, (ULONG)spacer, TAG_DONE);
    case 3:
        return MUI_NewObject(path, UPA_Tabs_Captions, (ULONG)g_tab_captions, TAG_DONE);
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
        g_row_hook.h_Entry = (__typeof__(g_row_hook.h_Entry))(void (*)(void))HookEntry;
        g_row_hook.h_SubEntry = (__typeof__(g_row_hook.h_SubEntry))(void (*)(void))demo_row;
        return MUI_NewObject(path, UPA_List_Count, 20UL, UPA_List_RowHook, (ULONG)(uintptr_t)&g_row_hook, TAG_DONE);
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
            if (i == 4) { /* I685w: the Balance is one gap wide; in a grid cell it sits in a horizontal group with a spacer so the cell has a shape */
                kids[k++].ti_Data = (ULONG)MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE, MUIA_Group_Child, (ULONG)obj[i],
                                                         MUIA_Group_Child, (ULONG)MUI_NewObject((char*)"UP-Text.mcc", UPA_Text_Contents, (ULONG) " ", TAG_DONE), TAG_DONE);
            } else {
                kids[k++].ti_Data = (ULONG)obj[i];
            }
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
            if (is_open && hold) { /* I685w: the rig census reads the widget boxes from here, in screen pixels */
                ULONG wptr = 0;
                GetAttr(MUIA_Window_Window, win, &wptr);
                Delay(50);
                if (wptr != 0) {
                    struct Window* w = (struct Window*)wptr;
                    for (i = 0; i < CLASS_COUNT; i++) {
                        ULONG l = 0, t = 0, ww = 0, hh = 0;
                        GetAttr(MUIA_LeftEdge, obj[i], &l);
                        GetAttr(MUIA_TopEdge, obj[i], &t);
                        GetAttr(MUIA_Width, obj[i], &ww);
                        GetAttr(MUIA_Height, obj[i], &hh);
                        printf("[RECT] %s %ld %ld %ld %ld\n", g_names[i], (long)(w->LeftEdge + l), (long)(w->TopEdge + t), (long)ww, (long)hh);
                    }
                    printf("[RECT] Window %ld %ld %ld %ld\n", (long)w->LeftEdge, (long)w->TopEdge, (long)w->Width, (long)w->Height);
                    fflush(stdout);
                }
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
