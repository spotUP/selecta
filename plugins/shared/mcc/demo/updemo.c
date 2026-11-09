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
static const char* const g_tab_captions[] = {"Songs", "Albums", "Artists", "Playlists", NULL};
static const char* const g_artists[] = {"Virgill", "Jogeir Liljedahl", "Purple Motion", "Zabutom", "Dubmood", "Chipzel"};
static const char* const g_times[] = {"3:42", "4:05", "2:58", "5:21", "3:09", "4:47"};
static UP_ListColumns g_columns = {3, {3, 2, 0}, {NULL, NULL, "0:00"}};

/* I685w run 3: three columns (title, artist, time) and a title row; the hook gets row -1 for the column titles. */
static struct Hook g_row_hook;
static ULONG demo_cell(struct Hook* hook, Object* obj, UP_ListCell* cell) {
    static char text[40];
    static const char* const heads[] = {"Title", "Artist", "Time"};
    (void)hook;
    (void)obj;
    if (cell->row < 0) {
        return (ULONG)(uintptr_t)heads[cell->col % 3];
    }
    if (cell->col == 0) {
        snprintf(text, sizeof text, "Track %ld", (long)cell->row + 1);
        return (ULONG)(uintptr_t)text;
    }
    return (ULONG)(uintptr_t)(cell->col == 1 ? g_artists[cell->row % 6] : g_times[cell->row % 6]);
}

/* A caption line in a tone. */
static Object* label(const char* text, ULONG tone) {
    return MUI_NewObject((char*)"UP-Text.mcc", UPA_Text_Contents, (ULONG)text, UPA_Text_Tone, tone, TAG_DONE);
}

/* A vertical or horizontal group of up to 6 children (NULLs skipped). Stock groups with MUIA_FillArea FALSE, so the panel face shows. */
static Object* box(ULONG horiz, Object* a, Object* b, Object* c, Object* d, Object* e, Object* f) {
    struct TagItem t[10];
    Object* kids[6];
    int i, n = 0;
    kids[0] = a, kids[1] = b, kids[2] = c, kids[3] = d, kids[4] = e, kids[5] = f;
    t[n].ti_Tag = MUIA_Group_Horiz, t[n++].ti_Data = horiz;
    t[n].ti_Tag = MUIA_FillArea, t[n++].ti_Data = (ULONG)FALSE;
    for (i = 0; i < 6; i++) {
        if (kids[i] != NULL) {
            t[n].ti_Tag = MUIA_Group_Child, t[n++].ti_Data = (ULONG)kids[i];
        }
    }
    t[n].ti_Tag = TAG_DONE, t[n].ti_Data = 0;
    return MUI_NewObjectA((char*)MUIC_Group, t);
}

/* A raised card: a heading line, then the widgets. */
static Object* card(const char* title, Object* a, Object* b, Object* c, Object* d) {
    return MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Raised, MUIA_Group_Child, (ULONG)label(title, UPV_Text_Tone_Title),
                         MUIA_Group_Child, (ULONG)box(FALSE, a, b, c, d, NULL, NULL), TAG_DONE);
}

static const char* const g_page_lines[4][2] = {{"Virgill - Past Tense", "Purple Motion - Second Reality"},
                                               {"Chipzel - Spectra", "Dubmood - Cheeky Little Buggers"},
                                               {"Zabutom", "Jogeir Liljedahl"},
                                               {"Morning mix", "Late night tracker tunes"}};

/* One page per tab: a page group whose active page follows UPA_Tabs_Active (I685w run 5). */
static Object* tab_pages(Object* tabs) {
    Object* pg[4];
    Object* grp;
    int i;
    for (i = 0; i < 4; i++) {
        pg[i] = box(FALSE, label(g_page_lines[i][0], UPV_Text_Tone_Normal), label(g_page_lines[i][1], UPV_Text_Tone_Muted), NULL, NULL, NULL, NULL);
    }
    grp = MUI_NewObject((char*)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, MUIA_Group_Child, (ULONG)pg[0], MUIA_Group_Child, (ULONG)pg[1], MUIA_Group_Child, (ULONG)pg[2],
                        MUIA_Group_Child, (ULONG)pg[3], TAG_DONE);
    if (grp != NULL) {
        DoMethod(tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, (ULONG)grp, 3UL, MUIM_Set, MUIA_Group_ActivePage, (ULONG)MUIV_TriggerValue);
    }
    return grp;
}

/* One instance of class `index` with the tags that class needs to be valid on its own; every class is created by file name. */
static Object* make(int index) {
    char path[32];
    snprintf(path, sizeof path, "UP-%s.mcc", g_names[index]);
    switch (index) {
    case 0:
        return MUI_NewObject(path, UPA_Button_Caption, (ULONG) "Play", UPA_Button_Icon, (ULONG) "play", UPA_Button_Role, (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    case 1:
        return MUI_NewObject(path, UPA_Text_Contents, (ULONG) "Welcome back. Every control below is a UP class loaded by name.", UPA_Text_Tone, (ULONG)UPV_Text_Tone_Muted, TAG_DONE);
    case 2:
        return MUI_NewObject(path, UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface, MUIA_Group_Child, (ULONG)label("Panel: a Surface role card", UPV_Text_Tone_Normal),
                             MUIA_Group_Child, (ULONG)label("It paints its face in the theme token.", UPV_Text_Tone_Muted), TAG_DONE);
    case 3:
        return MUI_NewObject(path, UPA_Tabs_Captions, (ULONG)g_tab_captions, TAG_DONE);
    case 4:
        return MUI_NewObject(path, TAG_DONE);
    case 5:
        return MUI_NewObject(path, MUIA_Numeric_Min, 0UL, MUIA_Numeric_Max, 100UL, MUIA_Numeric_Value, 40UL, TAG_DONE);
    case 6:
        return MUI_NewObject(path, UPA_Progress_Value, 62UL, UPA_Progress_Max, 100UL, TAG_DONE);
    case 7:
        return MUI_NewObject(path, UPA_Cycle_Entries, (ULONG)g_cycle_entries, TAG_DONE);
    case 8:
        return MUI_NewObject(path, UPA_Field_Placeholder, (ULONG) "Search songs, albums, artists", TAG_DONE);
    case 9:
        return MUI_NewObject(path, UPA_Scroller_Entries, 100UL, UPA_Scroller_Visible, 10UL, TAG_DONE);
    default:
        g_row_hook.h_Entry = (__typeof__(g_row_hook.h_Entry))(void (*)(void))HookEntry;
        g_row_hook.h_SubEntry = (__typeof__(g_row_hook.h_SubEntry))(void (*)(void))demo_cell;
        return MUI_NewObject(path, UPA_List_Count, 20UL, UPA_List_CellHook, (ULONG)(uintptr_t)&g_row_hook, UPA_List_Columns, (ULONG)(uintptr_t)&g_columns,
                             UPA_List_Title, (ULONG)TRUE, UPA_List_MaxRows, 5UL, TAG_DONE);
    }
}

int main(int argc, char** argv) {
    Object* obj[CLASS_COUNT];
    Object *app = NULL, *win, *root;
    ULONG sigs = 0, is_open = 0;
    int quit_now = argc > 1 && argv[1] != NULL && strcmp(argv[1], "QUIT") == 0;
    int hold = argc > 1 && argv[1] != NULL && strcmp(argv[1], "HOLD") == 0;
    int i, ok = 0, ticks = 0;

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
        {   /* I685w run 3: titled cards, the Balance between the two columns. Each of the 11 objects is in the tree once. */
            Object* ctl = card("Controls",
                               box(TRUE, obj[0], MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Shuffle", UPA_Button_Icon, (ULONG) "shuffle", TAG_DONE),
                                   MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Queue", UPA_Button_Role, (ULONG)UPV_Button_Role_Ghost, TAG_DONE),
                                   MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Role, (ULONG)UPV_Button_Role_Toggle, TAG_DONE), label("Repeat", UPV_Text_Tone_Normal), NULL),
                               box(FALSE, label("Search", UPV_Text_Tone_Muted), obj[8], obj[7], NULL, NULL, NULL), NULL, NULL);
            Object* left = card("Library", box(FALSE, obj[3], tab_pages(obj[3]), NULL, NULL, NULL, NULL), label("128 songs in your library", UPV_Text_Tone_Muted), obj[2], obj[10]);
            Object* right = card("Playback", label("Volume 40", UPV_Text_Tone_Muted), obj[5], label("Elapsed 2:05 of 3:22", UPV_Text_Tone_Muted),
                                 box(FALSE, obj[6], label("Scroll position", UPV_Text_Tone_Muted), obj[9], NULL, NULL, NULL));
            root = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Panel, MUIA_Group_Child, (ULONG)label("UP-Demo", UPV_Text_Tone_Heading),
                                 MUIA_Group_Child, (ULONG)obj[1], MUIA_Group_Child, (ULONG)ctl,
                                 MUIA_Group_Child, (ULONG)box(TRUE, left, obj[4], right, NULL, NULL, NULL), TAG_DONE);
        }
        win = root == NULL ? NULL
                           : MUI_NewObject((char*)MUIC_Window, MUIA_Window_Title, (ULONG) "UP-Demo", MUIA_Window_ID, 0x55504431, MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(90),
                                           MUIA_Window_Height, (ULONG)MUIV_Window_Height_Screen(90),
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
