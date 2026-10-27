/* Amiga MUI UI plugin: descriptor and hooks (plan 2026-10-25-amiga-mui.md MUI-P1-2). Same shape as the text and
 * desktop UI plugins. The MUI calls (MUI-P1-3) compile only on the Amiga with the MUI 3.8 headers; on the host this
 * file builds for the descriptor test and open always refuses. */
#include <selecta/engine.h>
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "map/mui_map.h"

#if defined(__amigaos__) || defined(AMIGA)
#define SEL_MUI_NATIVE 1
#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/muimaster_protos.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <devices/clipboard.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
struct Library* MUIMasterBase;
#include "mui_canvas_class.h"
#define SEL_MAKE_ID(a, b, c, d) (((ULONG)(a) << 24) | ((ULONG)(b) << 16) | ((ULONG)(c) << 8) | (ULONG)(d))

/* The SDK's sfdc inline MUI_NewObject is a function-like macro; the SDK's ApplicationObject/End idiom nests object-like
 * macros that end in an open call, which a function-like macro cannot span. So the varargs MUI_NewObject comes from
 * the toolchain's libmui.a (clib prototype, out-of-line stub that hands the caller's stacked tags to MUI_NewObjectA).
 * A local static varargs wrapper reading &tag1 is wrong: at -O3 gcc inlines it, &tag1 then points at a lone copy of
 * the first tag, the rest of the list is stack garbage, and the Window object never opens (I508). */
#undef MUI_NewObject
#endif

/* MUI-P1-4: a distinct error on stderr (the shell keeps the text UI) and, on the Amiga, an EasyRequest. */
static void mui_report(mui_fail why);

#define MUI_ROWS_MAX 64 /* most rows one window of the list holds (MUI-P3-1: only the engine window is inserted) */

/* One List entry. The title text comes first so that anything reading an entry as a string (the rig's gadget list) sees it. */
typedef struct mui_cells {
    char title[MUI_LIST_TEXT];
    char artist[64];
} mui_cells;

typedef struct mui_state {
    uint32_t opens; /* ui_open_mui sentinel (MUI-P1-5): windows opened */
    uint32_t wake_mask; /* engine wake signal (MUI-P1-7), 0 where there is none */
    uint32_t menu_sends; /* menu items that sent their action line to the engine (MUI-P2-4) */
    sel_engine* engine;
    uint32_t transport_hits; /* transport buttons and sliders that sent their line (MUI-P2-5): stat transport_hits */
    uint32_t row_plays;      /* list rows activated by double click (MUI-P3-1): stat row_plays */
    uint32_t drops;          /* AppWindow arguments sent to the engine (MUI-P5-6): stat drops */
    sel_tag stats[7];
    sel_uim uim;             /* the frame model the controls are driven from (MUI-P2-6) */
    char list_stat[160];     /* the list sentinel printed as ui_list: back end, style, columns, rows (native List only) */
#ifdef SEL_MUI_NATIVE
    Object* app;
    struct MsgPort* tport;          /* timer.device reply port: bounds the native wait by timeout_ms */
    struct timerequest* treq;       /* NULL when timer.device failed: the wait then falls back to Delay(1) */
    Object* win;
    Object* root;                   /* window contents: its MUIA_Width drives the breakpoint (MUI-P2-2) */
    Object* slot[MUI_SLOT_COUNT];   /* the five layout regions, created once, shown or hidden */
    Object* balance[2];             /* sidebar|main and main|detail */
    mui_layout_map_t lay;
    Object* canvas;                 /* MUI-P5-1 Selecta_Canvas, NULL when MUI refused the class */
    Object* btn[MUI_BTN_COUNT];     /* MUI-P2-5 transport buttons */
    const char* btn_label[MUI_BTN_COUNT];
    Object* seek;
    Object* vol;
    Object* txt[MUI_ATTR_COUNT];    /* MUI-P2-6 frame texts (NULL for the slider and button attributes) */
    char txt_buf[MUI_ATTR_COUNT][112];
    LONG seek_set, vol_set;         /* values last written by set(): a notify with another value is the user */
    Object* listview;               /* MUI-P3-1 */
    Object* list;
    mui_list_entry entries[MUI_ROWS_MAX];
    const char* entry_ptr[MUI_ROWS_MAX + 1];
    mui_cells cell[MUI_ROWS_MAX];   /* MUI-P3-4: the List entries; the display hook reads the cells from them */
    char col_fmt[32];               /* MUIA_List_Format of the shown column set (mui_columns_format) */
    uint32_t col_bp;                /* the breakpoint col_fmt was built for */
    Object* crumb;                  /* MUI-P3-3: the drilldown breadcrumb line above the list */
    char crumb_buf[128];
    uint32_t list_sig;
    LONG list_active_set;
    mui_list_pending list_pending;  /* the clicked row until the engine snapshot shows it */
#endif
} mui_state;

static mui_state g_state;
static sel_tag g_hooks[9];
static sel_tag g_desc[5];

static void put_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

#ifdef SEL_MUI_NATIVE
/* MUI-P2-3: the Menustrip built from the action table (mui_menu_build). MUI keeps the title and shortcut pointers, so the
 * rows and the one-character shortcut strings are static. A Menuitem trigger is a ReturnID (MENU_ID_BASE + row) that the
 * wait loop turns into the row's protocol line for the engine (MUI-P2-4). */
#define MENU_MAX 160
#define MENU_ID_BASE 0x1000L
#define CTL_CLIP_COPY 0x2120L /* Edit > Copy Song Title (MUI-P5-6), outside the MENU_ID_BASE + row range */
static mui_menu_item g_menu[MENU_MAX];
static char g_menu_sc[MENU_MAX][2];
static uint32_t g_menu_n;

static Object* menu_child(Object* parent, ULONG cls_is_menu, const char* title, Object** cache_key, const char** names, Object** objs,
                          uint32_t* count, const char* want) {
    uint32_t i;
    Object* o;
    (void)cls_is_menu;
    (void)cache_key;
    for (i = 0; i < *count; i++) {
        if (strcmp(names[i], want) == 0) {
            return objs[i];
        }
    }
    o = MUI_NewObject(cls_is_menu ? (char*)MUIC_Menu : (char*)MUIC_Menuitem,
                      cls_is_menu ? MUIA_Menu_Title : MUIA_Menuitem_Title, (ULONG)title, TAG_DONE);
    if (o != NULL) {
        DoMethod(parent, OM_ADDMEMBER, o);
        names[*count] = want; /* the lookup key (the same one compared above), not the title: else every item of a sub menu made its own */
        objs[*count] = o;
        (*count)++;
    }
    return o;
}

static Object* mui_menu_strip(void) {
    static const char* t_name[48];
    static Object* t_obj[48];
    static const char* s_name[96];
    static Object* s_obj[96];
    uint32_t nt = 0, ns = 0, i;
    Object* strip = MUI_NewObject((char*)MUIC_Menustrip, TAG_DONE);
    if (strip == NULL) {
        return NULL;
    }
    g_menu_n = mui_menu_build_for(g_menu, MENU_MAX, mui_style_mask(0)); /* MUI-P3-8; RTG widening: MUI-P6 */
    if (g_menu_n > MENU_MAX) {
        g_menu_n = MENU_MAX;
    }
    for (i = 0; i < g_menu_n; i++) {
        mui_menu_item* m = &g_menu[i];
        Object* menu = menu_child(strip, 1, m->title, NULL, t_name, t_obj, &nt, m->title);
        Object* parent = menu;
        Object* item;
        if (menu == NULL) {
            continue;
        }
        if (m->sub[0] != '\0') {
            static char key[96][80];
            char* k = key[ns < 96 ? ns : 95];
            snprintf(k, 80, "%s/%s", m->title, m->sub);
            parent = menu_child(menu, 0, m->sub, NULL, s_name, s_obj, &ns, k);
            if (parent == NULL) {
                continue;
            }
        }
        g_menu_sc[i][0] = m->shortcut;
        g_menu_sc[i][1] = '\0';
        item = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG)m->item,
                             MUIA_Menuitem_Shortcut, (ULONG)(m->shortcut != '\0' ? g_menu_sc[i] : NULL), TAG_DONE);
        if (item == NULL) {
            continue;
        }
        DoMethod(parent, OM_ADDMEMBER, item);
        DoMethod(item, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2,
                 MUIM_Application_ReturnID, (ULONG)(MENU_ID_BASE + (LONG)i));
    }
    {
        /* MUI-P5-6: Copy Song Title joins the Edit menu the table's undo and redo rows made. */
        Object* edit = menu_child(strip, 1, "Edit", NULL, t_name, t_obj, &nt, "Edit");
        Object* copy = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG) "Copy Song Title", MUIA_Menuitem_Shortcut,
                                     (ULONG) "C", TAG_DONE);
        if (edit != NULL && copy != NULL) {
            DoMethod(edit, OM_ADDMEMBER, copy);
            DoMethod(copy, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2,
                     MUIM_Application_ReturnID, (ULONG)CTL_CLIP_COPY);
        }
    }
    return strip;
}

/* Sends one command line. A decoded string argument points into the pool and the engine reads it on its next pump, after
 * this returns (the engine does not copy it): the pool outlives the call, a ring so several lines queued before one pump
 * keep their text. A stack pool left "VIEW_SOURCE search" with an empty source and the engine refused it (I569). */
static int mui_engine_line(mui_state* s, const char* line) {
    static char ring[16][512];
    static uint32_t next;
    char* pool = ring[next++ % 16u];
    return line != NULL && s->engine != NULL && sel_engine_send_line(s->engine, line, pool, sizeof ring[0]) == SEL_CODEC_OK;
}

/* A fired Menuitem: the same line the desktop sends for the action id (MUI-P2-4). */
static void mui_menu_fire(mui_state* s, uint32_t row) {
    const char* line = row < g_menu_n ? mui_menu_line(g_menu[row].id) : NULL;
    if (mui_engine_line(s, line)) {
        s->menu_sends++;
    }
}

/* MUI-P2-5/P2-6/P3-1: the controls. ReturnIDs: buttons, sliders and list events are turned into engine lines in
 * mui_control_fire, the same way a Menuitem is (mui_menu_fire). */
#define CTL_BTN_BASE 0x2000L
#define CTL_SEEK 0x2100L
#define CTL_VOLUME 0x2101L
#define CTL_LIST_DBL 0x2110L
#define CTL_LIST_SEL 0x2111L
#define CTL_DOOR_BASE 0x2200L /* MUI-P4-0: one button per sidebar door */
#define MUI_DOOR_MAX 8u

/* I564: a Text object reads "_" in its contents as the hotkey mark and drops it, so a file name such as
 * "that_meme_again.mod" shows as "thatmemeagain.mod". MUIA_Text_HiCharIdx names the mark character; MUI_NOMARK is a control
 * character no file name, title or label carries, so every "_" is shown verbatim. List rows do not parse the mark. */
#ifndef MUIA_Text_HiCharIdx
#define MUIA_Text_HiCharIdx 0x804214f5UL
#endif
#define MUI_NOMARK 1

static Object* mui_make_button(const char* label, LONG id) {
    Object* o = MUI_NewObject((char*)MUIC_Text, MUIA_Frame, MUIV_Frame_Button, MUIA_Font, MUIV_Font_Button,
                              MUIA_Text_Contents, (ULONG)label, MUIA_Text_HiCharIdx, MUI_NOMARK, MUIA_Text_PreParse, (ULONG) "\33c",
                              MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_Background, MUII_ButtonBack, TAG_DONE);
    if (o != NULL) {
        DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    }
    return o;
}

static Object* mui_make_text(mui_state* s, uint32_t attr, const char* initial) {
    snprintf(s->txt_buf[attr], sizeof s->txt_buf[attr], "%s", initial);
    s->txt[attr] = MUI_NewObject((char*)MUIC_Text, MUIA_Frame, MUIV_Frame_Text, MUIA_Text_Contents, (ULONG)s->txt_buf[attr], MUIA_Text_HiCharIdx, MUI_NOMARK,
                                 TAG_DONE);
    return s->txt[attr];
}

static Object* mui_make_slider(LONG max, LONG id) {
    Object* o = MUI_NewObject((char*)MUIC_Slider, MUIA_Slider_Horiz, TRUE, MUIA_Slider_Min, 0, MUIA_Slider_Max, max, TAG_DONE);
    if (o != NULL) {
        DoMethod(o, MUIM_Notify, MUIA_Slider_Level, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    }
    return o;
}

/* MUI-P4-0: the sidebar slot content, one button per door of the shared uimodel table (mui_door_label). */
static Object* mui_sidebar_group(void) {
    Object* g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    uint32_t i;
    for (i = 0; g != NULL && i < mui_door_count() && i < MUI_DOOR_MAX; i++) {
        Object* b = mui_make_button(mui_door_label(i), CTL_DOOR_BASE + (LONG)i);
        if (b != NULL) {
            DoMethod(g, OM_ADDMEMBER, b);
        }
    }
    if (g != NULL) {
        DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    }
    return g;
}

/* The transport row and frame texts: three rows of one VGroup (the PLAYER slot content). */
static Object* mui_player_group(mui_state* s) {
    sel_uim_frame zero;
    Object *row1, *row2, *row3, *g;
    uint32_t i;
    memset(&zero, 0, sizeof zero);
    row1 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    for (i = 0; row1 != NULL && i < MUI_BTN_COUNT; i++) {
        s->btn_label[i] = mui_transport_label(i, &zero);
        s->btn[i] = mui_make_button(s->btn_label[i], CTL_BTN_BASE + (LONG)i);
        if (s->btn[i] != NULL) {
            DoMethod(row1, OM_ADDMEMBER, s->btn[i]);
        }
    }
    s->vol = mui_make_slider(MUI_VOLUME_MAX, CTL_VOLUME);
    s->seek = mui_make_slider(MUI_SEEK_MAX, CTL_SEEK);
    row2 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    row3 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    if (row1 == NULL || row2 == NULL || row3 == NULL || g == NULL || s->vol == NULL || s->seek == NULL) {
        return NULL;
    }
    DoMethod(row1, OM_ADDMEMBER, s->vol);
    DoMethod(row2, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_CLOCK, "0:00 "));
    DoMethod(row2, OM_ADDMEMBER, s->seek);
    DoMethod(row2, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_REMAINING, "-0:00"));
    DoMethod(row2, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_LENGTH, "0:00 "));
    DoMethod(row3, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_TITLE, "No song"));
    DoMethod(row3, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_ARTIST, ""));
    DoMethod(row3, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_FORMAT, "        "));
    DoMethod(row3, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_STATUS, "Stopped"));
    DoMethod(g, OM_ADDMEMBER, row1);
    DoMethod(g, OM_ADDMEMBER, row2);
    DoMethod(g, OM_ADDMEMBER, row3);
    return g;
}

/* MUI-P3-4: the List's display hook. The entry is the row's cell array; a NULL entry asks for the column titles
 * (MUIA_List_Title TRUE). The columns shown are mui_columns_for(breakpoint): the rest are hidden by width. */
static struct Hook g_list_hook;
static uint32_t g_list_hook_calls;
static ULONG mui_list_display(struct Hook* hook, char** array, mui_cells* cells) {
    static const char* const titles[MUI_COL_COUNT] = {"Title", "Artist", "Album", "Length", "Format"};
    uint8_t cols[MUI_COL_COUNT];
    uint32_t n = mui_columns_for(g_state.col_bp, cols), k;
    (void)hook;
    g_list_hook_calls++;
    for (k = 0; k < n; k++) {
        const char* t = cells == NULL ? titles[cols[k]] : cols[k] == MUI_COL_TITLE ? cells->title : cols[k] == MUI_COL_ARTIST ? cells->artist : "";
        array[k] = (char*)t;
    }
    return 0;
}

/* MUI-P3-1: the 3.8 Listview with its plain List (the back end mui_list_backend_pick gives without NList). */
static Object* mui_main_group(mui_state* s) {
    Object* g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    s->col_bp = SEL_UIM_BP_COUNT;
    mui_columns_format(s->col_bp, s->col_fmt, sizeof s->col_fmt);
    g_list_hook.h_Entry = (__typeof__(g_list_hook.h_Entry))(void (*)(void))HookEntry;
    g_list_hook.h_SubEntry = (__typeof__(g_list_hook.h_SubEntry))(void (*)(void))mui_list_display;
    s->crumb = MUI_NewObject((char*)MUIC_Text, MUIA_Text_Contents, (ULONG)" ", MUIA_ShowMe, FALSE, TAG_DONE);
    s->list = MUI_NewObject((char*)MUIC_List, MUIA_Frame, MUIV_Frame_InputList, MUIA_List_DisplayHook, (ULONG)&g_list_hook,
                            MUIA_List_Format, (ULONG)s->col_fmt, MUIA_List_Title, TRUE, TAG_DONE);
    if (g == NULL || s->list == NULL || s->crumb == NULL) {
        return NULL;
    }
    s->listview = MUI_NewObject((char*)MUIC_Listview, MUIA_Listview_List, (ULONG)s->list, TAG_DONE);
    if (s->listview == NULL) {
        return NULL;
    }
    DoMethod(s->listview, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_LIST_DBL);
    DoMethod(s->list, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_LIST_SEL);
    s->list_active_set = -1;
    DoMethod(g, OM_ADDMEMBER, s->crumb);
    DoMethod(g, OM_ADDMEMBER, s->listview);
    /* MUI-P5-1: the canvas object sits beside the list, hidden until the style switch (MUI-P3-5) shows it. */
    {
        static const mui_grid_style grid = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 96, 8, 8};
        s->canvas = mui_canvas_new(&grid, 1);
        if (s->canvas != NULL) {
            DoMethod(g, OM_ADDMEMBER, s->canvas);
        }
    }
    return g;
}

/* One engine step into the controls: the frame through the dirty bits (mui_dirty_attrs) into set(), the button labels
 * (they depend on play, repeat and random state), and the list window when its rows changed. */
static void mui_refresh(mui_state* s) {
    const sel_state* st = sel_engine_state(s->engine);
    const sel_uim_frame* f;
    uint32_t attrs, a, i;
    const sel_view_state* v;
    if (st == NULL) {
        return;
    }
    s->uim.dirty |= sel_uim_update(&s->uim, st);
    attrs = mui_dirty_attrs(sel_uim_take_dirty(&s->uim));
    f = &s->uim.frame;
    for (a = 0; a < MUI_ATTR_COUNT; a++) {
        if ((attrs & (1u << a)) == 0) {
            continue;
        }
        if (s->txt[a] != NULL) {
            const char* t = mui_attr_text(a, f);
            snprintf(s->txt_buf[a], sizeof s->txt_buf[a], "%s", t != NULL && t[0] != '\0' ? t : " ");
            set(s->txt[a], MUIA_Text_Contents, (ULONG)s->txt_buf[a]);
        } else if (a == MUI_ATTR_SEEK) {
            s->seek_set = (LONG)f->progress_q16;
            set(s->seek, MUIA_Slider_Level, s->seek_set);
        } else if (a == MUI_ATTR_VOLUME) {
            s->vol_set = (LONG)f->volume;
            set(s->vol, MUIA_Slider_Level, s->vol_set);
        }
    }
    for (i = 0; i < MUI_BTN_COUNT; i++) {
        const char* label = mui_transport_label(i, f);
        if (label != NULL && label != s->btn_label[i]) {
            s->btn_label[i] = label;
            set(s->btn[i], MUIA_Text_Contents, (ULONG)label);
        }
    }
    v = st->view;
    if (v != NULL) {
        int32_t active = -1;
        uint32_t n = mui_list_build(mui_list_backend_pick(0, 0), s->entries, MUI_ROWS_MAX, v, &active);
        uint32_t sig = 2166136261u ^ v->window_start ^ (n << 20) ^ ((uint32_t)(active + 1) << 8) ^ (s->lay.breakpoint << 28);
        const char* c;
        active = mui_list_pending_resolve(&s->list_pending, active);
        if (s->lay.breakpoint != s->col_bp && s->lay.breakpoint < SEL_UIM_BP_COUNT) { /* MUI-P3-4: another column set */
            s->col_bp = s->lay.breakpoint;
            mui_columns_format(s->col_bp, s->col_fmt, sizeof s->col_fmt);
            set(s->list, MUIA_List_Format, (ULONG)s->col_fmt);
        }
        {
            uint32_t drill = v->style == SEL_VIEW_STYLE_DRILLDOWN; /* MUI-P3-3: the breadcrumb line, drilldown only */
            char crumb[sizeof s->crumb_buf];
            mui_list_breadcrumb(v, crumb, sizeof crumb);
            if (drill && strcmp(crumb, s->crumb_buf) != 0) {
                snprintf(s->crumb_buf, sizeof s->crumb_buf, "%s", crumb);
                set(s->crumb, MUIA_Text_Contents, (ULONG)s->crumb_buf);
            }
            set(s->crumb, MUIA_ShowMe, drill ? TRUE : FALSE);
        }
        for (i = 0; i < n; i++) {
            for (c = s->entries[i].text; *c != '\0'; c++) {
                sig = (sig ^ (uint8_t)*c) * 16777619u;
            }
            sig = (sig ^ 0xffu) * 16777619u;
        }
        if (sig != s->list_sig) {
            s->list_sig = sig;
            for (i = 0; i < n; i++) {
                snprintf(s->cell[i].title, sizeof s->cell[i].title, "%s", s->entries[i].text);
                snprintf(s->cell[i].artist, sizeof s->cell[i].artist, "%s", mui_row_cell(&v->rows[i], MUI_COL_ARTIST));
                s->entry_ptr[i] = (const char*)&s->cell[i];
            }
            s->entry_ptr[n] = NULL;
            set(s->list, MUIA_List_Quiet, TRUE);
            DoMethod(s->list, MUIM_List_Clear);
            if (n > 0) {
                DoMethod(s->list, MUIM_List_Insert, (ULONG)s->entry_ptr, (LONG)n, MUIV_List_Insert_Bottom);
            }
            s->list_active_set = active;
            set(s->list, MUIA_List_Active, active < 0 ? (LONG)MUIV_List_Active_Off : (LONG)active);
            set(s->list, MUIA_List_Quiet, FALSE);
        }
        /* The sentinel the exit stats print (ui_list): which back end, style and column set the List runs, and how often its display hook ran. */
        snprintf(s->list_stat, sizeof s->list_stat, "backend=%s style=%u cols=%u format=%s rows=%u indent=%u hook_calls=%u",
                 mui_list_backend_pick(0, 0)->name, (unsigned)v->style, (unsigned)mui_columns_for(s->col_bp, (uint8_t[MUI_COL_COUNT]){0}),
                 s->col_fmt, (unsigned)n, n > 1 ? (unsigned)(s->entries[n - 1].depth) : 0u, (unsigned)g_list_hook_calls);
    }
}

/* MUI-P5-6: write one FTXT image to clipboard.device unit 0 (CMD_WRITE, then CMD_UPDATE to post it). 1 when it landed. */
static int mui_clip_write(const uint8_t* iff, uint32_t n) {
    struct MsgPort* port = CreateMsgPort();
    struct IOClipReq* io = port != NULL ? (struct IOClipReq*)CreateIORequest(port, sizeof(struct IOClipReq)) : NULL;
    int ok = 0;
    if (io != NULL && OpenDevice((STRPTR) "clipboard.device", 0, (struct IORequest*)io, 0) == 0) {
        io->io_Data = (STRPTR)iff;
        io->io_Length = n;
        io->io_Offset = 0;
        io->io_ClipID = 0;
        io->io_Command = CMD_WRITE;
        ok = DoIO((struct IORequest*)io) == 0;
        if (ok) {
            io->io_Command = CMD_UPDATE;
            ok = DoIO((struct IORequest*)io) == 0;
        }
        CloseDevice((struct IORequest*)io);
    }
    if (io != NULL) {
        DeleteIORequest((struct IORequest*)io);
    }
    if (port != NULL) {
        DeleteMsgPort(port);
    }
    return ok;
}

/* Edit > Copy Song Title: "Artist - Title" of the shown song as FTXT text. */
static void mui_clip_copy(mui_state* s) {
    char text[200];
    uint8_t iff[240];
    uint32_t n = mui_clip_text(s->uim.frame.artist, s->uim.frame.title, text, sizeof text);
    uint32_t len = n != 0 ? mui_clip_iff(text, n, iff, sizeof iff) : 0;
    if (len != 0) {
        (void)mui_clip_write(iff, len);
    }
}

/* MUI-P5-6: the AppWindow drop. The window's MUIA_AppMessage notification calls this hook in the application task with
 * the AppMessage; each argument becomes QUEUE_ADD (file) or LIBRARY_SCAN (drawer), the lines the desktop's drop sends. */
static struct Hook g_drop_hook;
static mui_state* g_drop_state;

static ULONG mui_drop_hook(struct Hook* hook, Object* obj, struct AppMessage** msgp) {
    struct AppMessage* am = msgp != NULL ? *msgp : NULL;
    struct FileInfoBlock* fib;
    LONG i;
    (void)hook;
    (void)obj;
    if (am == NULL || g_drop_state == NULL || am->am_Type != AMTYPE_APPWINDOW) {
        return 0;
    }
    fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
    for (i = 0; fib != NULL && i < am->am_NumArgs; i++) {
        struct WBArg* a = &am->am_ArgList[i];
        char path[256], line[320];
        int is_dir = 0;
        if (a->wa_Lock == 0 || !NameFromLock(a->wa_Lock, (STRPTR)path, sizeof path)) {
            continue;
        }
        if (a->wa_Name != NULL && a->wa_Name[0] != '\0') {
            if (!AddPart((STRPTR)path, a->wa_Name, sizeof path)) {
                continue;
            }
            {
                BPTR l = Lock((STRPTR)path, ACCESS_READ);
                if (l != 0) {
                    is_dir = Examine(l, fib) && fib->fib_DirEntryType > 0;
                    UnLock(l);
                }
            }
        } else {
            is_dir = 1; /* a drawer icon: the lock is the drawer itself */
        }
        if (mui_drop_line(path, is_dir, line, sizeof line) != 0 && mui_engine_line(g_drop_state, line)) {
            g_drop_state->drops++;
        }
    }
    if (fib != NULL) {
        FreeDosObject(DOS_FIB, fib);
    }
    return 0;
}

/* A control's ReturnID: the line is the action table's (buttons, through mui_menu_line like the menu) or the shared
 * slider and list lines of the map layer. */
static void mui_control_fire(mui_state* s, LONG id) {
    char line[64];
    LONG v = 0;
    if (id >= CTL_BTN_BASE && id < CTL_BTN_BASE + (LONG)MUI_BTN_COUNT) {
        if (mui_engine_line(s, mui_menu_line(mui_transport_action((uint32_t)(id - CTL_BTN_BASE), &s->uim.frame)))) {
            s->transport_hits++;
        }
    } else if (id >= CTL_DOOR_BASE && id < CTL_DOOR_BASE + (LONG)MUI_DOOR_MAX) {
        mui_engine_line(s, mui_door_line((uint32_t)(id - CTL_DOOR_BASE))); /* the page switch: the engine moves the main view's source */
    } else if (id == CTL_CLIP_COPY) {
        mui_clip_copy(s);
    } else if (id == CTL_SEEK) {
        GetAttr(MUIA_Slider_Level, s->seek, (ULONG*)&v);
        if (v != s->seek_set && mui_seek_line((uint32_t)v, sel_engine_state(s->engine)->song.duration_ms, line, sizeof line)
            && mui_engine_line(s, line)) {
            s->transport_hits++;
        }
        s->seek_set = v;
    } else if (id == CTL_VOLUME) {
        GetAttr(MUIA_Slider_Level, s->vol, (ULONG*)&v);
        if (v != s->vol_set) {
            mui_volume_line((int32_t)v, line, sizeof line);
            if (mui_engine_line(s, line)) {
                s->transport_hits++;
            }
        }
        s->vol_set = v;
    } else if (id == CTL_LIST_SEL || id == CTL_LIST_DBL) {
        const sel_view_state* view = sel_engine_state(s->engine)->view;
        GetAttr(MUIA_List_Active, s->list, (ULONG*)&v);
        if (view == NULL || v < 0 || (id == CTL_LIST_SEL && v == s->list_active_set)) {
            return;
        }
        s->list_active_set = v;
        if (id == CTL_LIST_DBL && view->style == SEL_VIEW_STYLE_TREE && (uint32_t)v < view->row_n && view->rows[v].kind == SEL_ROW_NODE) {
            /* MUI-P3-2: a node row of the tree style toggles; the engine line is VIEW_EXPAND or VIEW_COLLAPSE <node id>. */
            mui_list_pending_clear(&s->list_pending);
            snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(view->window_start + (uint32_t)v));
            mui_engine_line(s, line);
            mui_list_toggle_line(&view->rows[v], line, sizeof line);
            mui_engine_line(s, line);
            return;
        }
        if (id == CTL_LIST_SEL) {
            int32_t shown = view->cursor >= view->window_start && view->cursor - view->window_start < view->window_rows
                                ? (int32_t)(view->cursor - view->window_start) : -1;
            mui_list_pending_arm(&s->list_pending, (int32_t)v, shown);
        } else {
            mui_list_pending_clear(&s->list_pending);
        }
        snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(view->window_start + (uint32_t)v));
        if (mui_engine_line(s, line) && id == CTL_LIST_DBL && mui_engine_line(s, "VIEW_ACTIVATE")) {
            s->row_plays++;
        }
    }
}

/* MUI-P2-1/P2-2: apply a layout map to the fixed objects: weights follow the region sizes, ShowMe hides what the
 * breakpoint drops. No object is created or disposed here. */
static void mui_apply_layout(mui_state* s) {
    uint32_t i;
    for (i = 0; i < MUI_SLOT_COUNT; i++) {
        set(s->slot[i], MUIA_ShowMe, s->lay.show[i] ? TRUE : FALSE);
        set(s->slot[i], MUIA_HorizWeight, s->lay.weight_w[i] > 0 ? s->lay.weight_w[i] : 1);
        set(s->slot[i], MUIA_VertWeight, s->lay.weight_h[i] > 0 ? s->lay.weight_h[i] : 1);
    }
    set(s->balance[0], MUIA_ShowMe, s->lay.balance_sidebar_main ? TRUE : FALSE);
    set(s->balance[1], MUIA_ShowMe, s->lay.balance_main_detail ? TRUE : FALSE);
}

/* The window size changed: recompute, and re-apply only when the breakpoint moved (weights are user-adjustable
 * through the Balance objects, so they are not forced back inside one breakpoint). */
static void mui_check_resize(mui_state* s) {
    ULONG w = 0, h = 0;
    uint32_t flips;
    if (s->root == NULL) {
        return;
    }
    GetAttr(MUIA_Width, s->root, &w);
    GetAttr(MUIA_Height, s->root, &h);
    if (w == 0 || (w == s->lay.width && h == s->lay.height)) {
        return;
    }
    if (mui_layout_relayout(&s->lay, (uint32_t)w, (uint32_t)h, &flips)) {
        mui_apply_layout(s);
    }
}

/* MUI-P1-3: muimaster.library v19 (MUI 3.8), one Application with one Window. The sentinel counts after the window
 * is open (MUI-P1-5), so ui_open_mui=1 proves the window really opened. */
static void mui_timer_open(mui_state* s);
static void mui_timer_close(mui_state* s);

static void* mui_open(const sel_ui_open_args* a) {
    mui_state* s = &g_state;
    ULONG open = 0;
    Object* strip;
    s->engine = a != NULL ? a->engine : NULL;
    MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, 19);
    if (MUIMasterBase == NULL) {
        mui_report(MUI_FAIL_NO_LIBRARY);
        return NULL;
    }
    strip = mui_menu_strip();
    sel_uim_init(&s->uim);
    s->app = ApplicationObject, MUIA_Application_Title, (ULONG) "Selecta", MUIA_Application_Base, (ULONG) "SELECTA",
        MUIA_Application_Version, (ULONG) "$VER: Selecta MUI 1.0",
        SubWindow, s->win = WindowObject, MUIA_Window_Title, (ULONG) "Selecta", MUIA_Window_ID, SEL_MAKE_ID('S', 'E', 'L', 'M'), MUIA_Window_AppWindow, TRUE,
            /* D-M4-3: full screen by default; width and height are 100 percent of the public screen area MUI can use.
             * The HVSpace child gives the group unbounded height: a lone Text object has a fixed maximum height and MUI
             * never sizes a window beyond its contents' maximum, which left the window 41 pixels high. */
            MUIA_Window_Width, MUIV_Window_Width_Screen(100), MUIA_Window_Height, MUIV_Window_Height_Screen(100),
            MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered, MUIA_Window_Menustrip, (ULONG)strip,
            WindowContents, s->root = VGroup,
                Child, s->slot[MUI_SLOT_SUGGESTION] = TextObject, MUIA_Text_Contents, (ULONG) "Window is narrow: try the mini layout", End,
                Child, HGroup,
                    Child, s->slot[MUI_SLOT_SIDEBAR] = mui_sidebar_group(),
                    Child, s->balance[0] = BalanceObject, End,
                    Child, s->slot[MUI_SLOT_MAIN] = mui_main_group(s),
                    Child, s->balance[1] = BalanceObject, End,
                    Child, s->slot[MUI_SLOT_DETAIL] = VGroup, Child, TextObject, MUIA_Text_Contents, (ULONG) "Details", End, Child, HVSpace, End,
                End,
                Child, s->slot[MUI_SLOT_PLAYER] = mui_player_group(s),
            End, End, End;
    if (s->app == NULL) {
        if (strip != NULL) {
            MUI_DisposeObject(strip);
        }
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
        mui_report(MUI_FAIL_NO_WINDOW);
        return NULL;
    }
    DoMethod(s->win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, s->app, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);
    g_drop_state = s;
    g_drop_hook.h_Entry = (__typeof__(g_drop_hook.h_Entry))(void (*)(void))HookEntry;
    g_drop_hook.h_SubEntry = (__typeof__(g_drop_hook.h_SubEntry))(void (*)(void))mui_drop_hook;
    DoMethod(s->win, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime, s->app, 3, MUIM_CallHook, (ULONG)&g_drop_hook, MUIV_TriggerValue);
    set(s->win, MUIA_Window_Open, TRUE);
    GetAttr(MUIA_Window_Open, s->win, &open); /* MUI opens the window inside the set; 0 here means it failed */
    if (!open) {
        MUI_DisposeObject(s->app);
        s->app = NULL;
        s->win = NULL;
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
        mui_report(MUI_FAIL_NO_WINDOW);
        return NULL;
    }
    s->wake_mask = 0;
    mui_timer_open(s);
    mui_layout_map(&s->lay, 1280, 720); /* the initial size is unknown until MUI lays out; the first resize check fixes it */
    mui_apply_layout(s);
    s->lay.width = 0;
    s->lay.breakpoint = SEL_UIM_BP_COUNT; /* no breakpoint yet: the first real size always applies */
    mui_check_resize(s);
    mui_refresh(s);
    s->opens++;
    return s;
}

static int mui_io_new_input(void* ctx, uint32_t* sigs) {
    ULONG got = 0;
    LONG id = (LONG)DoMethod(((mui_state*)ctx)->app, MUIM_Application_NewInput, &got);
    *sigs = (uint32_t)got;
    mui_check_resize((mui_state*)ctx);
    if (id >= MENU_ID_BASE && id < MENU_ID_BASE + (LONG)g_menu_n) {
        mui_menu_fire((mui_state*)ctx, (uint32_t)(id - MENU_ID_BASE));
    } else if (id >= CTL_BTN_BASE) {
        mui_control_fire((mui_state*)ctx, id);
    }
    mui_refresh((mui_state*)ctx);
    return id == (LONG)MUIV_Application_ReturnID_Quit ? MUI_WAIT_ID_QUIT : (int)id;
}

/* Sleep until a MUI signal, the engine wake, Ctrl-C or timeout_ms (timer.device, same pattern as the AGA desktop backend).
 * Wait() clears the signals it returns; the former SetSignal(0,0) poll only read them, so once any MUI port signal was set
 * it stayed set, Delay(1) never ran again and the idle task spun at 100 percent CPU. */
static void mui_timer_open(mui_state* s) {
    s->tport = CreateMsgPort();
    s->treq = s->tport ? (struct timerequest*)CreateIORequest(s->tport, sizeof *s->treq) : NULL;
    if (s->treq != NULL && OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest*)s->treq, 0) != 0) {
        DeleteIORequest((struct IORequest*)s->treq);
        s->treq = NULL;
    }
}

static void mui_timer_close(mui_state* s) {
    if (s->treq != NULL) {
        CloseDevice((struct IORequest*)s->treq);
        DeleteIORequest((struct IORequest*)s->treq);
        s->treq = NULL;
    }
    if (s->tport != NULL) {
        DeleteMsgPort(s->tport);
        s->tport = NULL;
    }
}

static uint32_t mui_io_wait(void* ctx, uint32_t mask, uint32_t timeout_ms) {
    mui_state* s = (mui_state*)ctx;
    ULONG got;
    if (s->treq == NULL || timeout_ms == 0u) {
        got = SetSignal(0, (ULONG)mask) & mask; /* read and clear what is already pending */
        if (got == 0 && s->treq == NULL) {
            Delay(1);
            got = SetSignal(0, (ULONG)mask) & mask;
        }
        return (uint32_t)got;
    }
    s->treq->tr_node.io_Command = TR_ADDREQUEST;
    s->treq->tr_time.tv_secs = timeout_ms / 1000u;
    s->treq->tr_time.tv_micro = (timeout_ms % 1000u) * 1000u;
    SendIO((struct IORequest*)s->treq);
    got = Wait((ULONG)mask | (1UL << s->tport->mp_SigBit));
    if (!CheckIO((struct IORequest*)s->treq)) {
        AbortIO((struct IORequest*)s->treq);
    }
    WaitIO((struct IORequest*)s->treq);
    return (uint32_t)(got & mask);
}

static int mui_wait(void* state, uint32_t timeout_ms) {
    mui_state* s = (mui_state*)state;
    mui_wait_io io;
    io.new_input = mui_io_new_input;
    io.wait = mui_io_wait;
    io.break_mask = SIGBREAKF_CTRL_C;
    io.wake_mask = s->wake_mask;
    return mui_wait_step(&io, s, timeout_ms);
}

static void mui_close(void* state) {
    mui_state* s = (mui_state*)state;
    if (s->app != NULL) {
        MUI_DisposeObject(s->app); /* closes the window and frees the children */
        s->app = NULL;
        s->win = NULL;
        s->canvas = NULL;
        mui_canvas_class_free();
    }
    mui_timer_close(s);
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
}

static int mui_native_probe(void) {
    struct Library* b = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, 19);
    if (b == NULL) {
        return 0;
    }
    CloseLibrary(b);
    return 1;
}

static void mui_report(mui_fail why) {
    struct EasyStruct es;
    const char* text = mui_fail_text(why);
    fprintf(stderr, "%s\n", text);
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE*)"Selecta";
    es.es_TextFormat = (UBYTE*)"%s";
    es.es_GadgetFormat = (UBYTE*)"Continue";
    EasyRequest(NULL, &es, NULL, (ULONG)text);
}
#else
static void mui_report(mui_fail why) {
    fprintf(stderr, "%s\n", mui_fail_text(why));
}

static void* mui_open(const sel_ui_open_args* a) {
    (void)a;
    mui_report(mui_present() ? MUI_FAIL_NO_WINDOW : MUI_FAIL_NO_LIBRARY);
    return NULL; /* the host has no MUI: the shell keeps its current UI */
}

static int mui_wait(void* state, uint32_t timeout_ms) {
    (void)state;
    (void)timeout_ms;
    return -1;
}

static void mui_close(void* state) {
    (void)state;
}
#endif

static const sel_tag* mui_stats(void* state) {
    mui_state* s = &g_state; /* one instance per process */
    (void)state;
    s->stats[0].tag = SEL_UI_STAT_OPEN_MUI;
    s->stats[0].value = s->opens;
    s->stats[1].tag = SEL_UI_STAT_MENUS;
    s->stats[1].value = s->menu_sends;
    s->stats[2].tag = SEL_UI_STAT_TRANSPORT_HITS;
    s->stats[2].value = s->transport_hits;
    s->stats[3].tag = SEL_UI_STAT_ROW_PLAYS;
    s->stats[3].value = s->row_plays;
    s->stats[4].tag = SEL_UI_STAT_DROPS;
    s->stats[4].value = s->drops;
    s->stats[5].tag = SEL_UI_STAT_LIST_VIEW;
    s->stats[5].value = (uintptr_t)s->list_stat;
    s->stats[6].tag = SEL_TAG_END;
    s->stats[6].value = 0;
    return s->stats;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
#ifdef SEL_MUI_NATIVE
    mui_set_probe(mui_native_probe);
#endif
    memset(&g_state, 0, sizeof g_state);
    put_tag(&g_hooks[0], SEL_UI_NAME, (uintptr_t) "MUI");
    put_tag(&g_hooks[1], SEL_UI_OPEN, SEL_TAG_FN(mui_open));
    put_tag(&g_hooks[2], SEL_UI_WAIT, SEL_TAG_FN(mui_wait));
    put_tag(&g_hooks[3], SEL_UI_CLOSE, SEL_TAG_FN(mui_close));
    put_tag(&g_hooks[4], SEL_UI_STATS, SEL_TAG_FN(mui_stats));
    /* Planar-safe mask: the descriptor is static, an RTG screen widens it when the window opens (MUI-P6). */
    put_tag(&g_hooks[5], SEL_UI_VIEW_STYLES, mui_style_mask(0));
    /* MUI 3.8 absent: still registered so an explicit --ui mui reaches mui_open and its requester, but not listed
     * or auto-picked (MUI-P1-6, Johan 2026-10-27). */
    put_tag(&g_hooks[6], SEL_UI_PRESENT, (uintptr_t)(mui_present() != 0));
    put_tag(&g_hooks[7], SEL_TAG_END, 0);

    put_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    put_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "mui");
    put_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    put_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    put_tag(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
