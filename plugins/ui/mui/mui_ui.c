/* Amiga MUI UI plugin: descriptor and hooks (plan 2026-10-25-amiga-mui.md MUI-P1-2). Same shape as the text and
 * desktop UI plugins. The MUI calls (MUI-P1-3) compile only on the Amiga with the MUI 3.8 headers; on the host this
 * file builds for the descriptor test and open always refuses. */
#include <selecta/engine.h>
#include <selecta/plugin.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map/mui_map.h"
#include "map/mui_theme.h"
#include "map/mui_icons.h"
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/theme.h>
#include <selecta/tags.h>

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
#include <proto/graphics.h>
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
static void mui_report(mui_fail why, const char* detail);

#define MUI_HELP_MAX 160 /* MUI-P4-14: most help rows shown (one per action table row) */
#define MUI_ROWS_MAX 64 /* most rows one window of the list holds (MUI-P3-1: only the engine window is inserted) */

/* One List entry. The title text comes first so that anything reading an entry as a string (the rig's gadget list) sees it. */
typedef struct mui_cells {
    char title[MUI_LIST_TEXT];
    char artist[64];
    mui_row_extra extra; /* album, length, format */
} mui_cells;

/* MUI-P4: the tool row gadgets and their counters in the ui_p4 sentinel. */
#define MUI_CRUMB_MAX 6u
enum { P4_SEARCH, P4_CMD, P4_LIKE, P4_RATING, P4_SUBSONG, P4_QREMOVE, P4_QCLEAR, P4_QUP, P4_QDOWN, P4_POPEN, P4_PRENAME, P4_PDELETE,
       P4_CLEARLIB, P4_HELP, P4_HOME, P4_TOAST, P4_CRUMB, P4_STATE, P4_SETTINGS, P4_SONGMENU, P4_NP, P4_SONGINFO, P4_PREF, P4_TRACKER,
       P4_COUNT };
static const char* const p4_name[P4_COUNT] = {"search", "cmd", "like", "rating", "subsong", "qremove", "qclear", "qup", "qdown",
                                              "popen", "prename", "pdelete", "clearlib", "help", "home", "toast", "crumb", "state", "settings",
                                              "songmenu", "np", "songinfo", "pref", "tracker"};
#define MUI_PREF_MAX 12u   /* MUI-P4-10: most Settings rows (sel_uim_pref_row_count) */
#define MUI_INFO_MAX 24u   /* MUI-P4-9: most song info rows */
#define MUI_TRK_ROWS 16u   /* MUI-P4-16: pattern rows shown */
#define MUI_TRK_CHANNELS 4u /* MUI-P4-16: channel columns shown */

typedef struct mui_state {
    uint32_t opens; /* ui_open_mui sentinel (MUI-P1-5): windows opened */
    uint32_t wake_mask; /* engine wake signal (MUI-P1-7), 0 where there is none */
    uint32_t menu_sends; /* menu items that sent their action line to the engine (MUI-P2-4) */
    sel_engine* engine;
    uint32_t transport_hits; /* transport buttons and sliders that sent their line (MUI-P2-5): stat transport_hits */
    uint32_t row_plays;      /* list rows activated by double click (MUI-P3-1): stat row_plays */
    uint32_t drops;          /* AppWindow arguments sent to the engine (MUI-P5-6): stat drops */
    sel_tag stats[8];
    char p4_stat[400];       /* MUI-P4: the sentinel printed as ui_p4: how often each P4 gadget sent its line */
    uint32_t p4[P4_COUNT];
    sel_uim uim;             /* the frame model the controls are driven from (MUI-P2-6) */
    char list_stat[480];     /* the list sentinel printed as ui_list: back end, style, columns, rows (native List only) */
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
    Object* btn_txt[MUI_BTN_COUNT];   /* I651: the label Text inside each transport button group */
    Object* btn_pages[MUI_BTN_COUNT]; /* I651: icon pages (play/pause), NULL for single-icon buttons */
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
    char col_fmt[48];               /* MUIA_List_Format of the shown column set (mui_columns_format) */
    uint32_t col_bp;                /* the breakpoint col_fmt was built for */
    Object* str_search;             /* MUI-P4-3 */
    Object* str_cmd;                /* MUI-P4-13 */
    Object* cyc_rating;             /* MUI-P4-7 */
    Object* cyc_subsong;            /* MUI-P4-9 */
    Object* status;                 /* the last line a P4 gadget sent (MUI-P4-11: the one-line toast) */
    mui_toast toast;                /* MUI-P4-11 */
    Object* help_win;               /* MUI-P4-14: created on the first Help click */
    char help_buf[MUI_HELP_MAX][96];
    const char* help_ptr[MUI_HELP_MAX + 1];
    char status_buf[160];
    Object* set_win;                /* MUI-P4-10: the Settings window, created on the first click on Settings */
    Object* set_ctl[MUI_PREF_MAX];
    char set_choice[MUI_PREF_MAX][80];
    const char* set_entry[MUI_PREF_MAX][10];
    Object* np_title;               /* MUI-P4-8 Now Playing group in the detail slot */
    Object* np_artist;
    Object* np_album;
    Object* np_lyrics;
    Object* np_up[3];
    char np_buf[3][112];
    char np_lyric_buf[1024];
    char np_up_buf[3][96];
    Object* info_list;              /* MUI-P4-9 song info rows */
    char info_buf[MUI_INFO_MAX][128];
    const char* info_ptr[MUI_INFO_MAX + 1];
    Object* trk_list;               /* MUI-P4-16 pattern rows, shown only when the engine answered with pattern data */
    Object* trk_group;
    char trk_buf[MUI_TRK_ROWS][160];
    const char* trk_ptr[MUI_TRK_ROWS + 1];
    uint32_t detail_sig, trk_sig;
    uint32_t pat_asked, pat_order, pat_row, pat_qid;
    uint32_t sm_playlist;           /* MUI-P4-6: the playlist last opened or seen, the target of Add to Playlist */
    uint32_t sm_on_queue;           /* the Song menu item 4 title follows the cursor row (Remove from Queue) */
    Object* crumb;                  /* MUI-P3-3: the drilldown breadcrumb line above the list */
    char crumb_buf[128];
    Object* crumb_row;              /* MUI-P4-2: the Library breadcrumb buttons (browse path pieces), shown in the browse source */
    Object* crumb_btn[MUI_CRUMB_MAX];
    char crumb_lbl[MUI_CRUMB_MAX][48];
    char crumb_path[256];           /* the browse path the buttons were last labelled from */
    Object* state_txt;              /* MUI-P4-15: the empty-state line of the view (sel_uim_state_text) */
    char state_buf[96];
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
static uint32_t mui_now_ms(void);
/* The tool button order is the label order of mui_tool_labels (map/mui_layout.c, tested on the host): this lists each button's id. */
static const uint32_t g_tool_which[MUI_TOOL_BTNS] = {P4_LIKE, P4_QREMOVE, P4_QCLEAR, P4_QUP, P4_QDOWN, P4_HELP, P4_SETTINGS, P4_POPEN, P4_PRENAME, P4_PDELETE, P4_CLEARLIB};
/* MUI-P2-3: the Menustrip built from the action table (mui_menu_build). MUI keeps the title and shortcut pointers, so the
 * rows and the one-character shortcut strings are static. A Menuitem trigger is a ReturnID (MENU_ID_BASE + row) that the
 * wait loop turns into the row's protocol line for the engine (MUI-P2-4). */
#define MENU_MAX 160
#define MENU_ID_BASE 0x1000L
#define CTL_SM_BASE 0x2500L /* MUI-P4-6: Song menu items, CTL_SM_BASE + item */
#define CTL_PREF_BASE 0x2600L /* MUI-P4-10: Settings gadgets, CTL_PREF_BASE + pref row */
#define CTL_CLIP_COPY 0x2120L /* Edit > Copy Song Title (MUI-P5-6), outside the MENU_ID_BASE + row range */
static mui_menu_item g_menu[MENU_MAX];
static char g_menu_sc[MENU_MAX][2];
static uint32_t g_menu_n;

/* MUI-P4-6: the five Song menu items (labels from sel_uim_song_menu_label, the desktop's set) under `parent`. They sit in the
 * window menu strip and in the Listview's context menu (right mouse). Both carry keys 1 to 5: while the pointer is over the Listview MUI
 * swaps the window strip for the context strip, so a strip without the keys leaves the shortcuts dead over the list. */
static Object* g_sm_item[2][SEL_UIM_SONG_MENU_ITEMS];
static void mui_song_menu_items(Object* parent, uint32_t which) {
    static const char* const keys[SEL_UIM_SONG_MENU_ITEMS] = {"1", "2", "3", "4", "5"};
    uint32_t i;
    for (i = 0; i < SEL_UIM_SONG_MENU_ITEMS; i++) {
        Object* it = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG)sel_uim_song_menu_label(i, 0),
                                   MUIA_Menuitem_Shortcut, (ULONG)keys[i], TAG_DONE);
        g_sm_item[which][i] = it;
        if (it != NULL) {
            DoMethod(parent, OM_ADDMEMBER, it);
            DoMethod(it, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
                     (ULONG)(CTL_SM_BASE + (LONG)i));
        }
    }
}

static Object* mui_song_context_menu(void) {
    Object* strip = MUI_NewObject((char*)MUIC_Menustrip, TAG_DONE);
    Object* menu = MUI_NewObject((char*)MUIC_Menu, MUIA_Menu_Title, (ULONG) "Song", TAG_DONE);
    if (strip == NULL || menu == NULL) {
        return NULL;
    }
    DoMethod(strip, OM_ADDMEMBER, menu);
    mui_song_menu_items(menu, 1u);
    return strip;
}

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
    {
        Object* song = menu_child(strip, 1, "Song", NULL, t_name, t_obj, &nt, "Song");
        if (song != NULL) {
            mui_song_menu_items(song, 0u);
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
#define CTL_CRUMB_BASE 0x2400L /* MUI-P4-2: one button per breadcrumb piece */
#define CTL_P4_BASE 0x2300L /* MUI-P4: CTL_P4_BASE + P4_* */
static const char* const p4_rating_entries[] = {"None", "1 star", "2 stars", "3 stars", "4 stars", "5 stars", NULL};
static const char* const p4_subsong_entries[] = {"Subsong 1", "Subsong 2", "Subsong 3", "Subsong 4", "Subsong 5", "Subsong 6", "Subsong 7", "Subsong 8", NULL};
#define CTL_EXTRA_BASE 0x2700L /* MUI-P8-1: album and speed buttons of the transport area */

/* I564: a Text object reads "_" in its contents as the hotkey mark and drops it, so a file name such as
 * "that_meme_again.mod" shows as "thatmemeagain.mod". MUIA_Text_HiCharIdx names the mark character; MUI_NOMARK is a control
 * character no file name, title or label carries, so every "_" is shown verbatim. List rows do not parse the mark. */
#ifndef MUIA_Text_HiCharIdx
#define MUIA_Text_HiCharIdx 0x804214f5UL
#endif
#define MUI_NOMARK 1


/* I648: the Selecta look. g_look is filled once at open from the txt_theme tokens (the desktop UI's table); an entry with
 * on == 0 leaves the user's MUI background and frame alone. TH_TAGS expands to two tag pairs: MUIA_Background and
 * MUIA_Frame, each TAG_IGNORE when the look is off, so the same call works either way. */
static struct {
    mui_theme_look look;
    int on;
} g_th[MUI_TH_COUNT];

#define TH_TAGS(role, frame_off) \
    g_th[role].on ? (ULONG)MUIA_Background : (ULONG)TAG_IGNORE, g_th[role].on ? (ULONG)g_th[role].look.background : 0UL, \
    (ULONG)MUIA_Frame, g_th[role].on ? (ULONG)g_th[role].look.frame : (ULONG)(frame_off),
#define TH_BG_OR(role, spec) MUIA_Background, g_th[role].on ? (ULONG)g_th[role].look.background : (ULONG)(spec),

/* Reads the screen's text pen colour (the user's MUI text pen is the DrawInfo text pen unless MUI prefs change it) and
 * switches every role on whose background keeps that text readable. */
static void mui_theme_init(void) {
    txt_theme th;
    ULONG rgb[3] = {0, 0, 0};
    uint32_t text = 0;
    struct Screen* scr = LockPubScreen(NULL);
    int i;
    if (scr != NULL) {
        struct DrawInfo* dri = GetScreenDrawInfo(scr);
        if (dri != NULL) {
            GetRGB32(scr->ViewPort.ColorMap, dri->dri_Pens[TEXTPEN], 1, rgb);
            FreeScreenDrawInfo(scr, dri);
            text = ((rgb[0] >> 24) << 16) | ((rgb[1] >> 24) << 8) | (rgb[2] >> 24);
        }
        UnlockPubScreen(NULL, scr);
    }
    txt_theme_default(&th);
    for (i = 0; i < MUI_TH_COUNT; i++) {
        g_th[i].on = mui_theme_look_for((mui_theme_role)i, th.token, &g_th[i].look) && mui_theme_readable(text, g_th[i].look.bg_rgb);
    }
}

/* I651: Bodychunk / Bitmap tags (MUI 3.8 libraries/mui.h carries them; guarded for older NDK copies). */
#ifndef MUIA_Bodychunk_Body
#define MUIA_Bodychunk_Body 0x8042ca67UL
#define MUIA_Bodychunk_Compression 0x8042de5fUL
#define MUIA_Bodychunk_Depth 0x8042c392UL
#define MUIA_Bodychunk_Masking 0x80423b0eUL
#define MUIA_Bitmap_Height 0x80421560UL
#define MUIA_Bitmap_SourceColors 0x80425360UL
#define MUIA_Bitmap_Transparent 0x80422805UL
#define MUIA_Bitmap_UseFriend 0x804239d8UL
#define MUIA_Bitmap_Width 0x8042eb3aUL
#endif

/* I651: the icon image of a button, a Bodychunk built from the build-time body. The palette is fixed at creation (theme
 * face and ink) and MUI remaps the bitmap once at layout; nothing is drawn per frame. Pen 0 is transparent so the button
 * face shows through. NULL when the icon is missing: the button then keeps its label only. */
static ULONG g_icon_pal[12];
static int g_icon_pal_set;

static Object* mui_icon_object(int icon) {
    const uint8_t* body = mui_icon_body(icon);
    if (body == NULL) {
        return NULL;
    }
    if (!g_icon_pal_set) {
        txt_theme th;
        uint32_t pal[12];
        int i;
        txt_theme_default(&th);
        mui_icon_palette(th.token, g_th[MUI_TH_BUTTON].on, pal);
        for (i = 0; i < 12; i++) {
            g_icon_pal[i] = pal[i];
        }
        g_icon_pal_set = 1;
    }
    return MUI_NewObject((char*)MUIC_Bodychunk, MUIA_FixWidth, MUI_ICON_PX, MUIA_FixHeight, MUI_ICON_PX, MUIA_Bitmap_Width, MUI_ICON_PX,
                         MUIA_Bitmap_Height, MUI_ICON_PX, MUIA_Bodychunk_Depth, 2, MUIA_Bodychunk_Body, (ULONG)body, MUIA_Bodychunk_Compression, 0,
                         MUIA_Bodychunk_Masking, 2, MUIA_Bitmap_Transparent, 0, MUIA_Bitmap_SourceColors, (ULONG)g_icon_pal, MUIA_Bitmap_UseFriend, TRUE,
                         TAG_DONE);
}

/* A button: a group (frame, background, press handling) holding the icon and the label text. alt_key names a second icon
 * shown on page 2 of a page-mode group (play/pause); *txt_out receives the label Text object so a later set() can change
 * it, *pages_out the page group (NULL without alt_key). weight 100 is the MUI default (equal shares of the row); 0 sizes
 * the button to its contents. Padding and icon size come from mui_icons.h. */
static Object* mui_make_button_i(const char* label, const char* icon_key, const char* alt_key, LONG id, LONG weight, int icon_only, Object** txt_out, Object** pages_out) {
    Object* img = mui_icon_object(mui_icon_id(icon_key));
    Object* alt = alt_key != NULL ? mui_icon_object(mui_icon_id(alt_key)) : NULL;
    Object* pages = NULL;
    /* I656: an icon-only button (dense rows) has no label text so its row fits the smallest screen; the label stays as the
     * ShortHelp. Without an icon it keeps the label. */
    Object* txt = icon_only && img != NULL ? NULL : MUI_NewObject((char*)MUIC_Text, MUIA_Font, MUIV_Font_Button, MUIA_Text_Contents, (ULONG)label, MUIA_Text_HiCharIdx, MUI_NOMARK,
                                MUIA_Text_PreParse, (ULONG) "\33c", TAG_DONE);
    Object* o = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_HorizSpacing, MUI_BTN_ICON_GAP, MUIA_Weight, (ULONG)weight,
                              TH_TAGS(MUI_TH_BUTTON, MUIV_Frame_Button) InnerSpacing(MUI_BTN_PAD_X, MUI_BTN_PAD_Y), MUIA_ShortHelp, (ULONG)label,
                              MUIA_InputMode, MUIV_InputMode_RelVerify, TH_BG_OR(MUI_TH_BUTTON, MUII_ButtonBack) TAG_DONE);
    if (o == NULL || (txt == NULL && !(icon_only && img != NULL))) {
        return o;
    }
    if (img != NULL && alt != NULL) {
        pages = MUI_NewObject((char*)MUIC_Group, MUIA_Group_PageMode, TRUE, MUIA_Group_Child, img, MUIA_Group_Child, alt, TAG_DONE);
        DoMethod(o, OM_ADDMEMBER, pages != NULL ? pages : img);
    } else if (img != NULL) {
        DoMethod(o, OM_ADDMEMBER, img);
    }
    if (txt != NULL) {
        DoMethod(o, OM_ADDMEMBER, txt);
    }
    DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    if (txt_out != NULL) {
        *txt_out = txt;
    }
    if (pages_out != NULL) {
        *pages_out = pages;
    }
    return o;
}

static Object* mui_make_button_w(const char* label, const char* icon_key, LONG id, LONG weight) {
    return mui_make_button_i(label, icon_key, NULL, id, weight, 0, NULL, NULL);
}

static Object* mui_make_button_icon(const char* label, const char* icon_key, LONG id) {
    return mui_make_button_i(label, icon_key, NULL, id, 100, 1, NULL, NULL);
}

static Object* mui_make_button(const char* label, const char* icon_key, LONG id) {
    return mui_make_button_w(label, icon_key, id, 100);
}

static Object* mui_make_text(mui_state* s, uint32_t attr, const char* initial) {
    snprintf(s->txt_buf[attr], sizeof s->txt_buf[attr], "%s", initial);
    s->txt[attr] = MUI_NewObject((char*)MUIC_Text, TH_TAGS(MUI_TH_STATUS, MUIV_Frame_Text) MUIA_Text_Contents, (ULONG)s->txt_buf[attr], MUIA_Text_HiCharIdx, MUI_NOMARK,
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
    Object* g = MUI_NewObject((char*)MUIC_Group, TH_TAGS(MUI_TH_PANEL, MUI_FRAME_NONE) TAG_DONE);
    uint32_t i;
    for (i = 0; g != NULL && i < mui_door_count() && i < MUI_DOOR_MAX; i++) {
        Object* b = mui_make_button(mui_door_label(i), sel_uim_door_at(i)->page, CTL_DOOR_BASE + (LONG)i);
        if (b != NULL) {
            DoMethod(g, OM_ADDMEMBER, b);
        }
    }
    if (g != NULL) {
        DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    }
    return g;
}

/* MUI-P8-1: the album and speed buttons, one row under the transport row. Labels and help come from the action table
 * (mui_extra_*); a press goes through mui_action_fire in mui_control_fire. */
static int mui_p8_send(void* ctx, const char* line) {
    return mui_engine_line((mui_state*)ctx, line);
}

static Object* mui_p8_extras_row(void) {
    Object* row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    uint32_t i;
    for (i = 0; row != NULL && i < MUI_EXTRA_COUNT; i++) {
        Object* b = mui_make_button_icon(mui_extra_label(i), mui_extra_action(i), CTL_EXTRA_BASE + (LONG)i);
        if (b != NULL) {
            SetAttrs(b, MUIA_ShortHelp, (ULONG)mui_extra_tip(i), TAG_DONE);
            DoMethod(row, OM_ADDMEMBER, b);
        }
    }
    return row;
}

/* The transport row and frame texts: three rows of one VGroup (the PLAYER slot content). */
static Object* mui_player_group(mui_state* s) {
    sel_uim_frame zero;
    Object *row1, *row1b, *row2, *row3, *g;
    uint32_t i;
    memset(&zero, 0, sizeof zero);
    row1 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    row1b = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE); /* I656: Repeat and Random, their state labels are long */
    for (i = 0; row1 != NULL && row1b != NULL && i < MUI_BTN_COUNT; i++) {
        s->btn_label[i] = mui_transport_label(i, &zero);
        s->btn[i] = mui_make_button_i(s->btn_label[i], mui_transport_action(i, &zero), i == MUI_BTN_PLAY ? "pause" : NULL, CTL_BTN_BASE + (LONG)i, 100, 0,
                                      &s->btn_txt[i], &s->btn_pages[i]);
        if (s->btn[i] != NULL) {
            DoMethod(i < MUI_TRANSPORT_ROW1 ? row1 : row1b, OM_ADDMEMBER, s->btn[i]);
        }
    }
    s->vol = mui_make_slider(MUI_VOLUME_MAX, CTL_VOLUME);
    s->seek = mui_make_slider(MUI_SEEK_MAX, CTL_SEEK);
    row2 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    row3 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    g = MUI_NewObject((char*)MUIC_Group, TH_TAGS(MUI_TH_PANEL, MUI_FRAME_NONE) TAG_DONE);
    if (row1 == NULL || row1b == NULL || row2 == NULL || row3 == NULL || g == NULL || s->vol == NULL || s->seek == NULL) {
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
    DoMethod(g, OM_ADDMEMBER, row1b);
    DoMethod(g, OM_ADDMEMBER, mui_p8_extras_row());
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
        const char* t = cells == NULL ? titles[cols[k]] : cols[k] == MUI_COL_TITLE ? cells->title : cols[k] == MUI_COL_ARTIST ? cells->artist : cols[k] == MUI_COL_ALBUM ? cells->extra.album : cols[k] == MUI_COL_LENGTH ? cells->extra.length : cells->extra.format;
        array[k] = (char*)t;
    }
    return 0;
}

/* The column titles the display hook returns (cells NULL), joined by '|': the rig's read-back of the header row (ui_list header=). */
static void mui_list_header(char* out, size_t cap) {
    char* arr[MUI_COL_COUNT];
    uint32_t k, n = mui_columns_for(g_state.col_bp, (uint8_t[MUI_COL_COUNT]){0});
    size_t used = 0;
    out[0] = '\0';
    for (k = 0; k < MUI_COL_COUNT; k++) {
        arr[k] = (char*)"";
    }
    (void)mui_list_display(&g_list_hook, arr, NULL);
    g_list_hook_calls--; /* a read-back is not a display call */
    for (k = 0; k < n && used + 1u < cap; k++) {
        used += (size_t)snprintf(out + used, cap - used, "%s%s", k > 0 ? "|" : "", arr[k]);
    }
}

/* MUI-P3-1: the 3.8 Listview with its plain List (the back end mui_list_backend_pick gives without NList). */
static Object* mui_make_string(LONG id) {
    Object* o = MUI_NewObject((char*)MUIC_String, TH_TAGS(MUI_TH_FIELD, MUIV_Frame_String) MUIA_String_MaxLen, 159, MUIA_String_Contents, (ULONG) "", TAG_DONE);
    if (o != NULL) {
        DoMethod(o, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    }
    return o;
}

static Object* mui_make_cycle(const char* const* entries, LONG id) {
    Object* o = MUI_NewObject((char*)MUIC_Cycle, MUIA_Cycle_Entries, (ULONG)entries, TAG_DONE);
    if (o != NULL) {
        DoMethod(o, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
    }
    return o;
}

/* MUI-P4: the tool rows above the list: search and command line Strings, the like button, the rating and subsong Cycles,
 * the queue and playlist actions of the cursor row, and Clear Library (confirmed by a MUI_Request). */
static Object* mui_tool_group(mui_state* s) {
    Object* g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    Object* r1 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    Object* r2 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    Object* r3 = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    uint32_t i;
    if (g == NULL || r1 == NULL || r2 == NULL || r3 == NULL) {
        return NULL;
    }
    s->str_search = mui_make_string(CTL_P4_BASE + P4_SEARCH);
    s->str_cmd = mui_make_string(CTL_P4_BASE + P4_CMD);
    s->cyc_rating = mui_make_cycle(p4_rating_entries, CTL_P4_BASE + P4_RATING);
    s->cyc_subsong = mui_make_cycle(p4_subsong_entries, CTL_P4_BASE + P4_SUBSONG);
    snprintf(s->status_buf, sizeof s->status_buf, "Ready");
    s->status = MUI_NewObject((char*)MUIC_Text, TH_TAGS(MUI_TH_STATUS, MUIV_Frame_Text) MUIA_Text_Contents, (ULONG)s->status_buf, MUIA_Text_HiCharIdx, MUI_NOMARK, TAG_DONE);
    if (s->str_search == NULL || s->str_cmd == NULL || s->cyc_rating == NULL || s->cyc_subsong == NULL || s->status == NULL) {
        return NULL;
    }
    DoMethod(r1, OM_ADDMEMBER, s->str_search);
    DoMethod(r1, OM_ADDMEMBER, s->str_cmd);
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        Object* b = mui_make_button_icon(mui_tool_labels[i], mui_tool_labels[i], CTL_P4_BASE + (LONG)g_tool_which[i]);
        if (b != NULL) {
            DoMethod(i < MUI_TOOL_ROW2 ? r2 : r3, OM_ADDMEMBER, b);
        }
    }
    DoMethod(r3, OM_ADDMEMBER, s->cyc_rating);
    DoMethod(r3, OM_ADDMEMBER, s->cyc_subsong);
    DoMethod(g, OM_ADDMEMBER, r1);
    DoMethod(g, OM_ADDMEMBER, r2);
    DoMethod(g, OM_ADDMEMBER, r3);
    DoMethod(g, OM_ADDMEMBER, s->status);
    return g;
}

static Object* mui_main_group(mui_state* s) {
    Object* g = MUI_NewObject((char*)MUIC_Group, TH_TAGS(MUI_TH_PANEL, MUI_FRAME_NONE) TAG_DONE);
    s->col_bp = SEL_UIM_BP_COUNT;
    mui_columns_format(s->col_bp, s->col_fmt, sizeof s->col_fmt);
    g_list_hook.h_Entry = (__typeof__(g_list_hook.h_Entry))(void (*)(void))HookEntry;
    g_list_hook.h_SubEntry = (__typeof__(g_list_hook.h_SubEntry))(void (*)(void))mui_list_display;
    s->crumb = MUI_NewObject((char*)MUIC_Text, MUIA_Text_Contents, (ULONG)" ", MUIA_ShowMe, FALSE, TAG_DONE);
    s->list = MUI_NewObject((char*)MUIC_List, TH_TAGS(MUI_TH_LIST, MUIV_Frame_InputList) MUIA_List_DisplayHook, (ULONG)&g_list_hook,
                            MUIA_List_Format, (ULONG)s->col_fmt, MUIA_List_Title, TRUE, TAG_DONE);
    if (g == NULL || s->list == NULL || s->crumb == NULL) {
        return NULL;
    }
    s->listview = MUI_NewObject((char*)MUIC_Listview, MUIA_Listview_List, (ULONG)s->list, TAG_DONE);
    if (s->listview == NULL) {
        return NULL;
    }
    set(s->listview, MUIA_ContextMenu, (ULONG)mui_song_context_menu()); /* MUI-P4-6: right mouse button */
    DoMethod(s->listview, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_LIST_DBL);
    DoMethod(s->list, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_LIST_SEL);
    s->list_active_set = -1;
    {
        Object* tools = mui_tool_group(s);
        if (tools == NULL) {
            return NULL;
        }
        DoMethod(g, OM_ADDMEMBER, tools);
    }
    DoMethod(g, OM_ADDMEMBER, s->crumb);
    s->crumb_row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_ShowMe, FALSE, TAG_DONE);
    s->state_txt = MUI_NewObject((char*)MUIC_Text, MUIA_Text_Contents, (ULONG)" ", MUIA_ShowMe, FALSE, TAG_DONE);
    if (s->crumb_row == NULL || s->state_txt == NULL) {
        return NULL;
    }
    {
        uint32_t k;
        for (k = 0; k < MUI_CRUMB_MAX; k++) {
            snprintf(s->crumb_lbl[k], sizeof s->crumb_lbl[k], " ");
            s->crumb_btn[k] = mui_make_button_w(s->crumb_lbl[k], "crumb", CTL_CRUMB_BASE + (LONG)k, 0);
            if (s->crumb_btn[k] == NULL) {
                return NULL;
            }
            set(s->crumb_btn[k], MUIA_ShowMe, FALSE);
            DoMethod(s->crumb_row, OM_ADDMEMBER, s->crumb_btn[k]);
        }
    }
    DoMethod(s->crumb_row, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    DoMethod(g, OM_ADDMEMBER, s->crumb_row);
    DoMethod(g, OM_ADDMEMBER, s->state_txt);
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
/* MUI-P4-8/9/16: the detail slot. Now Playing (artwork placeholder, title, artist, album, lyrics, next in queue), the song
 * information rows, and the tracker pattern rows. All text comes from the engine state; mui_detail_refresh refills it when the
 * signature of the shown values changes. */
static Object* mui_np_text(char* buf, size_t cap, const char* init) {
    snprintf(buf, cap, "%s", init);
    return MUI_NewObject((char*)MUIC_Text, MUIA_Text_Contents, (ULONG)buf, TAG_DONE);
}

static Object* mui_detail_group(mui_state* s) {
    Object* g = MUI_NewObject((char*)MUIC_Group, TH_TAGS(MUI_TH_PANEL, MUI_FRAME_NONE) TAG_DONE);
    Object* art = MUI_NewObject((char*)MUIC_Rectangle, MUIA_Frame, MUIV_Frame_Group, MUIA_FixHeight, 64, MUIA_FixWidth, 64, TAG_DONE);
    Object* lyr_list = MUI_NewObject((char*)MUIC_Floattext, MUIA_Floattext_Text, (ULONG)s->np_lyric_buf, TAG_DONE);
    Object* lyr_view;
    uint32_t i;
    if (g == NULL) {
        return NULL;
    }
    s->np_lyric_buf[0] = '\0';
    s->np_title = mui_np_text(s->np_buf[0], sizeof s->np_buf[0], " ");
    s->np_artist = mui_np_text(s->np_buf[1], sizeof s->np_buf[1], " ");
    s->np_album = mui_np_text(s->np_buf[2], sizeof s->np_buf[2], " ");
    s->np_lyrics = lyr_list;
    lyr_view = lyr_list != NULL ? MUI_NewObject((char*)MUIC_Listview, MUIA_Listview_List, (ULONG)lyr_list, TAG_DONE) : NULL;
    s->info_list = MUI_NewObject((char*)MUIC_List, TH_TAGS(MUI_TH_LIST, MUIV_Frame_InputList) TAG_DONE);
    s->trk_list = MUI_NewObject((char*)MUIC_List, TH_TAGS(MUI_TH_LIST, MUIV_Frame_InputList) TAG_DONE);
    s->trk_group = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
    if (art != NULL) {
        DoMethod(g, OM_ADDMEMBER, art);
    }
    DoMethod(g, OM_ADDMEMBER, s->np_title);
    DoMethod(g, OM_ADDMEMBER, s->np_artist);
    DoMethod(g, OM_ADDMEMBER, s->np_album);
    if (lyr_view != NULL) {
        DoMethod(g, OM_ADDMEMBER, lyr_view);
    }
    DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Text, MUIA_Text_Contents, (ULONG) "\033cNext in Queue", TAG_DONE));
    for (i = 0; i < 3; i++) {
        s->np_up[i] = mui_np_text(s->np_up_buf[i], sizeof s->np_up_buf[i], " ");
        DoMethod(g, OM_ADDMEMBER, s->np_up[i]);
    }
    if (s->info_list != NULL) {
        DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Listview, MUIA_Listview_List, (ULONG)s->info_list, TAG_DONE));
    }
    if (s->trk_group != NULL && s->trk_list != NULL) {
        DoMethod(s->trk_group, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Listview, MUIA_Listview_List, (ULONG)s->trk_list, TAG_DONE));
        DoMethod(g, OM_ADDMEMBER, s->trk_group);
    }
    return g;
}

static const char* mui_base_name(const char* path) {
    const char* b = path, *p;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            b = p + 1;
        }
    }
    return b;
}

static void mui_flat(char* d, size_t cap, const char* src) {
    size_t i = 0;
    for (; src != NULL && src[i] != '\0' && i + 1 < cap; i++) {
        d[i] = (unsigned char)src[i] < 32u ? ' ' : src[i];
    }
    d[i] = '\0';
}

static void mui_detail_refresh(mui_state* s, const sel_state* st) {
    const sel_song_state* so = &st->song;
    const sel_pattern_view* pv = st->pattern;
    uint32_t sig, i, n, k;
    const char* title, *artist, *album, *lyr;
    if (s->np_title == NULL) {
        return;
    }
    /* The pattern window follows the play position: ask again when the order or the row moved. */
    if (so->path != NULL && (so->pattern_pos.order != s->pat_order || so->pattern_pos.row != s->pat_row || s->pat_asked == 0u)) {
        s->pat_order = so->pattern_pos.order;
        s->pat_row = so->pattern_pos.row;
        s->pat_asked = 1u;
        sel_engine_pattern_request(s->engine, s->pat_order, s->pat_row, MUI_TRK_ROWS);
    }
    sig = 2166136261u;
#define MUI_SIG(v) (sig = (sig ^ (uint32_t)(v)) * 16777619u)
    for (k = 0; so->path != NULL && so->path[k] != '\0'; k++) {
        MUI_SIG(so->path[k]);
    }
    MUI_SIG(so->subsong);
    MUI_SIG(so->subsong_count);
    MUI_SIG(st->lyric_line);
    MUI_SIG(st->lyric_lines);
    MUI_SIG(st->queue_playing);
    MUI_SIG(st->queue_count);
    MUI_SIG(pv != NULL ? pv->seq : 0u);
    if (sig == s->detail_sig) {
        return;
    }
    s->detail_sig = sig;
    title = so->path != NULL ? (const char*)sel_tag_find(so->meta, SEL_META_TITLE, 0) : NULL;
    artist = so->path != NULL ? (const char*)sel_tag_find(so->meta, SEL_META_AUTHOR, 0) : NULL;
    album = so->path != NULL ? (const char*)sel_tag_find(so->meta, SEL_META_RELEASE, 0) : NULL;
    if ((title == NULL || title[0] == '\0') && so->path != NULL) {
        title = mui_base_name(so->path);
    }
    mui_flat(s->np_buf[0], sizeof s->np_buf[0], title != NULL && title[0] != '\0' ? title : " ");
    mui_flat(s->np_buf[1], sizeof s->np_buf[1], artist != NULL && artist[0] != '\0' ? artist : " ");
    mui_flat(s->np_buf[2], sizeof s->np_buf[2], album != NULL && album[0] != '\0' ? album : " ");
    set(s->np_title, MUIA_Text_Contents, (ULONG)s->np_buf[0]);
    set(s->np_artist, MUIA_Text_Contents, (ULONG)s->np_buf[1]);
    set(s->np_album, MUIA_Text_Contents, (ULONG)s->np_buf[2]);
    /* Lyrics: plain text whole; synced lyrics show the current line and the next. */
    lyr = st->lyric_text;
    if (lyr != NULL) {
        snprintf(s->np_lyric_buf, sizeof s->np_lyric_buf, "%s", lyr);
    } else if (st->lyric_current != NULL || st->lyric_next != NULL) {
        snprintf(s->np_lyric_buf, sizeof s->np_lyric_buf, "%s\n%s", st->lyric_current != NULL ? st->lyric_current : "",
                 st->lyric_next != NULL ? st->lyric_next : "");
    } else {
        s->np_lyric_buf[0] = '\0';
    }
    set(s->np_lyrics, MUIA_Floattext_Text, (ULONG)s->np_lyric_buf);
    /* Next in queue: the three entries after the playing one. */
    n = 0;
    for (i = 0; i < st->queue_count && st->queue != NULL; i++) {
        if (st->queue[i].id == st->queue_playing) {
            n = i + 1u;
            break;
        }
    }
    for (i = 0; i < 3u; i++) {
        const char* p = n + i < st->queue_count && st->queue != NULL ? st->queue_text + st->queue[n + i].path_offset : NULL;
        mui_flat(s->np_up_buf[i], sizeof s->np_up_buf[i], p != NULL ? mui_base_name(p) : " ");
        set(s->np_up[i], MUIA_Text_Contents, (ULONG)s->np_up_buf[i]);
    }
    if (title != NULL && title[0] != '\0') {
        s->p4[P4_NP]++;
    }
    /* Song information: one "Label: value" row per meta tag the song carries, then the subsong line. */
    if (s->info_list != NULL) {
        uint32_t tag, rows = 0;
        DoMethod(s->info_list, MUIM_List_Clear);
        for (tag = SEL_META_TITLE; so->path != NULL && sel_tag_name(tag) != NULL && rows < MUI_INFO_MAX - 1u; tag++) {
            sel_tag_type ty;
            char lab[48], val[96];
            uintptr_t v = sel_tag_find(so->meta, tag, 0);
            if (v == 0 || sel_tag_type_of(tag, &ty) == 0) {
                continue;
            }
            sel_uim_tag_label(tag, lab, sizeof lab);
            if (ty == SEL_TT_STR) {
                mui_flat(val, sizeof val, (const char*)v);
            } else if (ty == SEL_TT_INT) {
                snprintf(val, sizeof val, "%lu", (unsigned long)v);
            } else {
                continue;
            }
            snprintf(s->info_buf[rows], sizeof s->info_buf[rows], "%s: %s", lab, val);
            s->info_ptr[rows] = s->info_buf[rows];
            rows++;
        }
        if (so->path != NULL && so->subsong_count > 1u) {
            snprintf(s->info_buf[rows], sizeof s->info_buf[rows], "Subsong: %lu of %lu", (unsigned long)so->subsong + 1ul,
                     (unsigned long)so->subsong_count);
            s->info_ptr[rows] = s->info_buf[rows];
            rows++;
        }
        s->info_ptr[rows] = NULL;
        if (rows > 0u) {
            DoMethod(s->info_list, MUIM_List_Insert, (ULONG)s->info_ptr, (LONG)rows, MUIV_List_Insert_Bottom);
            s->p4[P4_SONGINFO]++;
        }
    }
    /* Tracker pattern rows, only when the engine answered with pattern data. */
    if (s->trk_list != NULL) {
        uint32_t rows = 0;
        DoMethod(s->trk_list, MUIM_List_Clear);
        if (pv != NULL && pv->valid != 0u) {
            uint32_t ch = pv->channels < MUI_TRK_CHANNELS ? pv->channels : MUI_TRK_CHANNELS;
            for (i = 0; i < pv->row_n && rows < MUI_TRK_ROWS; i++) {
                char* o = s->trk_buf[rows];
                size_t used;
                uint32_t c;
                snprintf(o, sizeof s->trk_buf[rows], "%02lX", (unsigned long)(pv->row_lo + i));
                for (c = 0; c < ch; c++) {
                    char cell[40];
                    used = strlen(o);
                    mui_tracker_cell_text(&pv->cell[i][c], cell, sizeof cell);
                    snprintf(o + used, sizeof s->trk_buf[rows] - used, " | %s", cell);
                }
                s->trk_ptr[rows] = o;
                rows++;
            }
            s->trk_ptr[rows] = NULL;
            DoMethod(s->trk_list, MUIM_List_Insert, (ULONG)s->trk_ptr, (LONG)rows, MUIV_List_Insert_Bottom);
            if (rows > 0u) {
                s->p4[P4_TRACKER]++;
            }
        }
        set(s->trk_group, MUIA_ShowMe, rows > 0u ? TRUE : FALSE);
    }
}

static void mui_refresh(mui_state* s) {
    const sel_state* st = sel_engine_state(s->engine);
    const sel_uim_frame* f;
    uint32_t attrs, a, i;
    const sel_view_state* v;
    if (st == NULL) {
        return;
    }
    if (mui_toast_tick(&s->toast, mui_now_ms())) { /* MUI-P4-11: the toast went away after 3 s (6 s for an error) */
        snprintf(s->status_buf, sizeof s->status_buf, "Ready");
        set(s->status, MUIA_Text_Contents, (ULONG)s->status_buf);
        s->p4[P4_TOAST]++;
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
            set(s->btn_txt[i], MUIA_Text_Contents, (ULONG)label);
            if (s->btn_pages[i] != NULL) {
                const char* act = mui_transport_action(i, f);
                set(s->btn_pages[i], MUIA_Group_ActivePage, act != NULL && strcmp(act, "pause") == 0 ? 1 : 0);
            }
        }
    }
    mui_detail_refresh(s, st);
    v = st->view;
    if (v != NULL) {
        int32_t active = -1;
        uint32_t n = mui_list_build(mui_list_backend_pick(0, 0), s->entries, MUI_ROWS_MAX, v, &active);
        uint32_t sig = 2166136261u ^ v->window_start ^ (n << 20) ^ ((uint32_t)(active + 1) << 8) ^ (s->lay.breakpoint << 28) ^ (st->song.duration_ms * 2654435761u) ^ st->song.queue_id;
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
        {
            /* MUI-P4-2: the browse path as clickable pieces; MUI-P4-15: the empty-state line of a view without rows. */
            mui_crumb pieces[MUI_CRUMB_MAX];
            const char* bp = v->source == SEL_VIEW_BROWSE && v->browse_path != NULL ? v->browse_path : "";
            uint32_t k, np;
            const char* stxt;
            ULONG parent = 0;
            GetAttr(MUIA_Parent, s->crumb_row, &parent);
            if (strcmp(bp, s->crumb_path) != 0 && parent != 0) {
                DoMethod((Object*)parent, MUIM_Group_InitChange);
                DoMethod(s->crumb_row, MUIM_Group_InitChange);
                snprintf(s->crumb_path, sizeof s->crumb_path, "%s", bp);
                np = mui_path_crumbs(bp, pieces, MUI_CRUMB_MAX);
                for (k = 0; k < MUI_CRUMB_MAX; k++) {
                    if (k < np) {
                        snprintf(s->crumb_lbl[k], sizeof s->crumb_lbl[k], "%s", pieces[k].name);
                        set(s->crumb_btn[k], MUIA_Text_Contents, (ULONG)s->crumb_lbl[k]);
                    }
                    set(s->crumb_btn[k], MUIA_ShowMe, k < np ? TRUE : FALSE);
                }
                set(s->crumb_row, MUIA_ShowMe, np > 0 ? TRUE : FALSE);
                DoMethod(s->crumb_row, MUIM_Group_ExitChange);
                DoMethod((Object*)parent, MUIM_Group_ExitChange);
            }
            stxt = mui_view_is_empty(v) ? sel_uim_state_text(mui_state_view_of(v->source), SEL_UIM_ST_EMPTY) : NULL;
            if (stxt == NULL) {
                stxt = "";
            }
            if (strcmp(stxt, s->state_buf) != 0) {
                ULONG sparent = 0;
                snprintf(s->state_buf, sizeof s->state_buf, "%s", stxt);
                GetAttr(MUIA_Parent, s->state_txt, &sparent);
                /* A ShowMe change needs the group's layout pass, like the crumb row above. */
                if (sparent != 0) {
                    DoMethod((Object*)sparent, MUIM_Group_InitChange);
                }
                set(s->state_txt, MUIA_Text_Contents, (ULONG)(s->state_buf[0] != '\0' ? s->state_buf : " "));
                set(s->state_txt, MUIA_ShowMe, s->state_buf[0] != '\0' ? TRUE : FALSE);
                if (sparent != 0) {
                    DoMethod((Object*)sparent, MUIM_Group_ExitChange);
                }
                if (s->state_buf[0] != '\0') {
                    s->p4[P4_STATE]++;
                }
            }
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
                {
                    char rpath[256], grp[64];
                    grp[0] = '\0';
                    if (sel_engine_view_row_path(s->engine, i, rpath, sizeof rpath)) {
                        (void)sel_engine_song_group(s->engine, rpath, grp, sizeof grp);
                    }
                    mui_row_extra_fill(st, &v->rows[i], grp, &s->cell[i].extra);
                }
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
        {
            char head[160], row0[320]; /* row0 lists all five cells of the first row, shown or hidden by width */
            mui_list_header(head, sizeof head);
            if (n > 0) {
                snprintf(row0, sizeof row0, "%s|%s|%s|%s|%s", s->cell[0].title, s->cell[0].artist, s->cell[0].extra.album, s->cell[0].extra.length, s->cell[0].extra.format);
            } else {
                row0[0] = '\0';
            }
            snprintf(s->list_stat, sizeof s->list_stat, "backend=%s style=%u cols=%u format=%s rows=%u indent=%u hook_calls=%u song_qid=%u song_ms=%u row0_id=%u header={%s} row0={%s}",
                     mui_list_backend_pick(0, 0)->name, (unsigned)v->style, (unsigned)mui_columns_for(s->col_bp, (uint8_t[MUI_COL_COUNT]){0}),
                     s->col_fmt, (unsigned)n, n > 1 ? (unsigned)(s->entries[n - 1].depth) : 0u, (unsigned)g_list_hook_calls, (unsigned)st->song.queue_id, (unsigned)st->song.duration_ms, n > 0 ? (unsigned)v->rows[0].id : 0u, head, row0);
        }
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
/* MUI-P4: one P4 gadget fired. The line comes from the shared mapping (map/mui_*.c); the count is the ui_p4 sentinel. */
static const sel_view_row* mui_cursor_row(mui_state* s, const sel_view_state** vout) {
    const sel_view_state* view = sel_engine_state(s->engine)->view;
    *vout = view;
    if (view == NULL || view->cursor < view->window_start || view->cursor - view->window_start >= view->row_n) {
        return NULL;
    }
    return &view->rows[view->cursor - view->window_start];
}

/* Milliseconds from dos DateStamp (wraps; the toast code compares differences only). */
static uint32_t mui_now_ms(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return (uint32_t)ds.ds_Minute * 60000u + (uint32_t)ds.ds_Tick * 20u + (uint32_t)ds.ds_Days * 86400000u;
}

static void mui_p4_note(mui_state* s, uint32_t which, const char* line) {
    s->p4[which]++;
    snprintf(s->status_buf, sizeof s->status_buf, "%s: %s", p4_name[which], line);
    mui_toast_show(&s->toast, s->status_buf, 0, mui_now_ms());
    set(s->status, MUIA_Text_Contents, (ULONG)s->status_buf);
}

/* MUI-P4-14: the Help window, one List row per action table row (mui_help_row). Built once, then only opened. */
static void mui_help_open(mui_state* s) {
    uint32_t i, n = mui_help_count();
    Object* lst;
    if (s->help_win == NULL) {
        if (n > MUI_HELP_MAX) {
            n = MUI_HELP_MAX;
        }
        for (i = 0; i < n; i++) {
            mui_help_row(i, s->help_buf[i], sizeof s->help_buf[i]);
            s->help_ptr[i] = s->help_buf[i];
        }
        s->help_ptr[n] = NULL;
        lst = MUI_NewObject((char*)MUIC_List, TH_TAGS(MUI_TH_LIST, MUIV_Frame_InputList) TAG_DONE);
        if (lst == NULL) {
            return;
        }
        s->help_win = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Title, (ULONG) "Selecta Help", MUIA_Window_ID, 0x48454C50UL,
                                    MUIA_Window_RootObject, (ULONG)MUI_NewObject((char*)MUIC_Listview, MUIA_Listview_List, (ULONG)lst, TAG_DONE), TAG_DONE);
        if (s->help_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->help_win);
        DoMethod(lst, MUIM_List_Insert, (ULONG)s->help_ptr, (LONG)n, MUIV_List_Insert_Bottom);
    }
    set(s->help_win, MUIA_Window_Open, TRUE);
}

static int mui_cmd_send(void* ctx, const char* line) {
    return mui_engine_line((mui_state*)ctx, line);
}

/* MUI-P4-6: one Song menu item fired, on the song under the list cursor. The lines come from the shared mapping. */
static void mui_song_menu_fire(mui_state* s, uint32_t item) {
    const sel_view_state* view;
    const sel_view_row* row = mui_cursor_row(s, &view);
    char path[512], line[640];
    const char* follow;
    uint32_t qid = 0, pl = s->sm_playlist, i;
    int on_queue;
    if (row == NULL || (row->kind != (uint32_t)SEL_ROW_FILE && row->kind != (uint32_t)SEL_ROW_QUEUE)) {
        return;
    }
    on_queue = row->kind == (uint32_t)SEL_ROW_QUEUE;
    qid = on_queue ? row->id : 0u;
    if (sel_engine_view_row_path(s->engine, view->cursor - view->window_start, path, sizeof path) <= 0) {
        return;
    }
    for (i = 0; view->source == SEL_VIEW_FOLDERS && i < view->row_n; i++) {
        if (view->rows[i].kind == (uint32_t)SEL_ROW_PLAYLIST) {
            pl = view->rows[i].id;
            break;
        }
    }
    if (item == 4u && !on_queue) {
        char msg[200];
        mui_confirm_text(0, mui_base_name(path), 0, 0, msg, sizeof msg);
        if (MUI_Request(s->app, s->win, 0, "Selecta", "*Remove|Cancel", "%s", (ULONG)msg) != 1) {
            return;
        }
    }
    if (mui_song_menu_line(item, path, qid, pl, on_queue, line, sizeof line) == 0u || !mui_engine_line(s, line)) {
        return;
    }
    follow = mui_song_menu_follow(item);
    if (follow != NULL) {
        mui_engine_line(s, follow);
    }
    mui_p4_note(s, P4_SONGMENU, line);
}

/* MUI-P4-10: the Settings window, one row per sel_uim_pref_row. Built on the first open; each gadget change sends PREF_SET. */
static Object* mui_pref_gadget(mui_state* s, uint32_t i, const sel_uim_pref_row* r) {
    const sel_host* host = sel_engine_plugin_host(s->engine);
    const char* cur = host != NULL && host->config_get != NULL ? host->config_get(host, r->key, "") : "";
    Object* o = NULL;
    LONG id = CTL_PREF_BASE + (LONG)i;
    if (cur == NULL) {
        cur = "";
    }
    if (r->ui == (uint32_t)SEL_UIM_PREF_CYCLE) {
        uint32_t n = 0, act = 0;
        char* p;
        snprintf(s->set_choice[i], sizeof s->set_choice[i], "%s", r->choices);
        for (p = s->set_choice[i]; n < 9u;) {
            s->set_entry[i][n++] = p;
            p = strchr(p, ',');
            if (p == NULL) {
                break;
            }
            *p++ = '\0';
        }
        s->set_entry[i][n] = NULL;
        for (act = 0; act < n; act++) {
            if (strcmp(s->set_entry[i][act], cur) == 0) {
                break;
            }
        }
        o = mui_make_cycle(s->set_entry[i], id);
        if (o != NULL && act < n) {
            set(o, MUIA_Cycle_Active, (ULONG)act);
        }
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {
        o = MUI_NewObject((char*)MUIC_Image, MUIA_Image_Spec, MUII_CheckMark, MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Selected,
                          (ULONG)(cur[0] == '1' ? TRUE : FALSE), MUIA_ShowSelState, FALSE, MUIA_Frame, MUIV_Frame_ImageButton, TAG_DONE);
        if (o != NULL) {
            DoMethod(o, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
        }
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_SLIDER) {
        long lo = 0, hi = 100;
        sscanf(r->choices, "%ld,%ld", &lo, &hi);
        o = MUI_NewObject((char*)MUIC_Slider, MUIA_Slider_Horiz, TRUE, MUIA_Slider_Min, (LONG)lo, MUIA_Slider_Max, (LONG)hi, MUIA_Slider_Level,
                          cur[0] != '\0' ? (LONG)strtol(cur, NULL, 10) : (LONG)lo, TAG_DONE);
        if (o != NULL) {
            DoMethod(o, MUIM_Notify, MUIA_Slider_Level, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)id);
        }
    } else {
        o = mui_make_string(id);
        if (o != NULL) {
            set(o, MUIA_String_Contents, (ULONG)cur);
        }
    }
    return o;
}

static void mui_settings_open(mui_state* s) {
    uint32_t i, n = sel_uim_pref_row_count();
    Object* grid;
    if (s->set_win == NULL) {
        if (n > MUI_PREF_MAX) {
            n = MUI_PREF_MAX;
        }
        grid = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Columns, 2, TAG_DONE);
        if (grid == NULL) {
            return;
        }
        for (i = 0; i < n; i++) {
            const sel_uim_pref_row* r = sel_uim_pref_row_at(i);
            Object* g = r != NULL ? mui_pref_gadget(s, i, r) : NULL;
            if (g == NULL) {
                continue;
            }
            s->set_ctl[i] = g;
            DoMethod(grid, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Text, MUIA_Text_Contents, (ULONG)r->label, MUIA_Text_PreParse, (ULONG) "\033r", TAG_DONE));
            DoMethod(grid, OM_ADDMEMBER, g);
        }
        s->set_win = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Title, (ULONG) "Selecta Settings", MUIA_Window_ID, 0x53455453UL,
                                   MUIA_Window_RootObject, (ULONG)grid, TAG_DONE);
        if (s->set_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->set_win);
    }
    set(s->set_win, MUIA_Window_Open, TRUE);
}

/* A Settings gadget changed: build the value the pref row expects and send PREF_SET key "value". */
static void mui_pref_fire(mui_state* s, uint32_t i) {
    const sel_uim_pref_row* r = i < MUI_PREF_MAX ? sel_uim_pref_row_at(i) : NULL;
    ULONG v = 0;
    char val[160], line[300];
    if (r == NULL || s->set_ctl[i] == NULL) {
        return;
    }
    val[0] = '\0';
    if (r->ui == (uint32_t)SEL_UIM_PREF_CYCLE) {
        GetAttr(MUIA_Cycle_Active, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%s", v < 9u && s->set_entry[i][v] != NULL ? s->set_entry[i][v] : "");
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {
        GetAttr(MUIA_Selected, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%d", v ? 1 : 0);
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_SLIDER) {
        GetAttr(MUIA_Slider_Level, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%ld", (long)v);
    } else {
        GetAttr(MUIA_String_Contents, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%s", v != 0 ? (const char*)v : "");
    }
    sel_uim_pref_line(r->key, val, line, sizeof line);
    if (mui_engine_line(s, line)) {
        mui_p4_note(s, P4_PREF, line);
    }
}

static void mui_p4_fire(mui_state* s, uint32_t which) {
    char line[200], lines[3][160];
    const sel_view_state* view;
    const sel_view_row* row = mui_cursor_row(s, &view);
    ULONG text = 0, act = 0;
    uint32_t n, i;
    switch (which) {
    case P4_SEARCH:
        GetAttr(MUIA_String_Contents, s->str_search, &text);
        n = mui_search_lines((const char*)text, lines);
        for (i = 0; i < n; i++) {
            mui_engine_line(s, lines[i]);
        }
        if (n > 0) {
            mui_p4_note(s, which, lines[0]);
        }
        break;
    case P4_CMD:
        GetAttr(MUIA_String_Contents, s->str_cmd, &text);
        if (mui_cmdline_submit(mui_cmd_send, s, (const char*)text)) {
            mui_p4_note(s, which, (const char*)text);
        }
        break;
    case P4_LIKE:
        if (mui_engine_line(s, mui_like_line())) {
            mui_p4_note(s, which, mui_like_line());
        }
        break;
    case P4_RATING:
        GetAttr(MUIA_Cycle_Active, s->cyc_rating, &act);
        mui_rating_line((uint32_t)act, line, sizeof line);
        if (mui_engine_line(s, line)) {
            mui_p4_note(s, which, line);
        }
        break;
    case P4_SUBSONG:
        GetAttr(MUIA_Cycle_Active, s->cyc_subsong, &act);
        if (mui_subsong_cycle_line((uint32_t)act, s->uim.frame.subsong_count, line, sizeof line) && mui_engine_line(s, line)) {
            mui_p4_note(s, which, line);
        }
        break;
    case P4_QCLEAR:
        mui_queue_line("clear", 0, 0, line, sizeof line);
        if (mui_engine_line(s, line)) {
            mui_p4_note(s, which, line);
        }
        break;
    case P4_QREMOVE:
    case P4_QUP:
    case P4_QDOWN:
        if (row != NULL && row->kind == (uint32_t)SEL_ROW_QUEUE) {
            uint32_t at = view->cursor - view->window_start;
            const sel_view_row* other = which == P4_QUP ? (at > 0 ? &view->rows[at - 1] : NULL) : (at + 1 < view->row_n ? &view->rows[at + 1] : NULL);
            if (which == P4_QREMOVE) {
                mui_queue_line("remove", row->id, 0, line, sizeof line);
            } else if (other != NULL) {
                mui_queue_line("move", row->id, other->id, line, sizeof line);
            } else {
                break;
            }
            if (mui_engine_line(s, line)) {
                mui_p4_note(s, which, line);
            }
        }
        break;
    case P4_POPEN:
    case P4_PRENAME:
    case P4_PDELETE:
        if (row != NULL && row->kind == (uint32_t)SEL_ROW_PLAYLIST) {
            GetAttr(MUIA_String_Contents, s->str_cmd, &text);
            s->sm_playlist = row->id;
            if (mui_playlist_line(which == P4_POPEN ? "open" : which == P4_PRENAME ? "rename" : "delete", row->id, (const char*)text, line, sizeof line)
                && mui_engine_line(s, line)) {
                mui_p4_note(s, which, line);
            }
        }
        break;
    case P4_SETTINGS:
        mui_settings_open(s);
        mui_p4_note(s, which, "Settings window");
        break;
    case P4_HELP:
        mui_help_open(s);
        mui_p4_note(s, which, "Help window");
        break;
    case P4_CLEARLIB: {
        /* MUI-P4-12: the confirm text is shared; MUI_Request returns 1 for the first (Clear) button. */
        char msg[200];
        mui_confirm_text(1, "Clear Library", 0, 0, msg, sizeof msg);
        if (MUI_Request(s->app, s->win, 0, "Selecta", "*Clear|Cancel", "%s", (ULONG)msg) == 1 && mui_engine_line(s, mui_confirm_line(1))) {
            mui_p4_note(s, which, mui_confirm_line(1));
        }
        break;
    }
    default:
        break;
    }
}

static void mui_control_fire(mui_state* s, LONG id) {
    char line[64];
    LONG v = 0;
    if (id >= CTL_BTN_BASE && id < CTL_BTN_BASE + (LONG)MUI_BTN_COUNT) {
        if (mui_engine_line(s, mui_menu_line(mui_transport_action((uint32_t)(id - CTL_BTN_BASE), &s->uim.frame)))) {
            s->transport_hits++;
        }
    } else if (id >= CTL_EXTRA_BASE && id < CTL_EXTRA_BASE + (LONG)MUI_EXTRA_COUNT) {
        if (mui_action_fire(mui_extra_action((uint32_t)(id - CTL_EXTRA_BASE)), mui_p8_send, s)) {
            s->transport_hits++;
        }
    } else if (id >= CTL_DOOR_BASE && id < CTL_DOOR_BASE + (LONG)MUI_DOOR_MAX) {
        mui_engine_line(s, mui_door_line((uint32_t)(id - CTL_DOOR_BASE))); /* the page switch: the engine moves the main view's source */
    } else if (id >= CTL_CRUMB_BASE && id < CTL_CRUMB_BASE + (LONG)MUI_CRUMB_MAX) {
        /* MUI-P4-2: a crumb opens the browse path up to and including that piece. */
        const sel_view_state* view = sel_engine_state(s->engine)->view;
        mui_crumb pieces[MUI_CRUMB_MAX];
        uint32_t k = (uint32_t)(id - CTL_CRUMB_BASE);
        if (view != NULL && view->source == SEL_VIEW_BROWSE && view->browse_path != NULL && k < mui_path_crumbs(view->browse_path, pieces, MUI_CRUMB_MAX)) {
            char cl[300];
            snprintf(cl, sizeof cl, "LIBRARY_BROWSE %.*s", (int)pieces[k].end, view->browse_path);
            if (mui_engine_line(s, cl)) {
                s->p4[P4_CRUMB]++;
            }
        }
    } else if (id >= CTL_SM_BASE && id < CTL_SM_BASE + (LONG)SEL_UIM_SONG_MENU_ITEMS) {
        mui_song_menu_fire(s, (uint32_t)(id - CTL_SM_BASE));
    } else if (id >= CTL_PREF_BASE && id < CTL_PREF_BASE + (LONG)MUI_PREF_MAX) {
        mui_pref_fire(s, (uint32_t)(id - CTL_PREF_BASE));
    } else if (id >= CTL_P4_BASE && id < CTL_P4_BASE + (LONG)P4_COUNT) {
        mui_p4_fire(s, (uint32_t)(id - CTL_P4_BASE));
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
        if (id == CTL_LIST_DBL && (uint32_t)v < view->row_n) {
            char hl[3][48];
            if (mui_home_lines(view, (uint32_t)v, hl) == 3) { /* MUI-P4-1: a Home row plays the song or opens the album */
                mui_list_pending_clear(&s->list_pending);
                mui_engine_line(s, hl[0]);
                mui_engine_line(s, hl[1]);
                mui_engine_line(s, hl[2]);
                s->p4[P4_HOME]++;
                return;
            }
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
        mui_report(MUI_FAIL_NO_LIBRARY, "");
        return NULL;
    }
    mui_theme_init();
    strip = mui_menu_strip();
    sel_uim_init(&s->uim);
    /* MUI-P4-17: MUIA_Application_Base is the settings identity only. The shell opened the one ARexx port SELECTA before this
     * UI (arexx_open, apps/selecta/main.c), so MUI takes SELECTA.1 for its own built-in commands and no MUIA_Application_Commands
     * table is given: PLAY and every other command reach the engine through the shell's port, one command set (plan section 5). */
    s->app = ApplicationObject, MUIA_Application_Title, (ULONG) "Selecta", MUIA_Application_Base, (ULONG) "SELECTA",
        MUIA_Application_Version, (ULONG) "$VER: Selecta MUI 1.0",
        SubWindow, s->win = WindowObject, MUIA_Window_Title, (ULONG) "Selecta", MUIA_Window_ID, SEL_MAKE_ID('S', 'E', 'L', 'M'), MUIA_Window_AppWindow, TRUE,
            /* D-M4-3: full screen by default; width and height are 100 percent of the public screen area MUI can use.
             * The HVSpace child gives the group unbounded height: a lone Text object has a fixed maximum height and MUI
             * never sizes a window beyond its contents' maximum, which left the window 41 pixels high. */
            MUIA_Window_Width, MUIV_Window_Width_Screen(100), MUIA_Window_Height, MUIV_Window_Height_Screen(100),
            MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered, MUIA_Window_Menustrip, (ULONG)strip,
            WindowContents, s->root = VGroup, TH_TAGS(MUI_TH_WINDOW, MUI_FRAME_NONE)
                Child, s->slot[MUI_SLOT_SUGGESTION] = TextObject, MUIA_Text_Contents, (ULONG) "Window is narrow: try the mini layout", End,
                Child, HGroup,
                    Child, s->slot[MUI_SLOT_SIDEBAR] = mui_sidebar_group(),
                    Child, s->balance[0] = BalanceObject, End,
                    Child, s->slot[MUI_SLOT_MAIN] = mui_main_group(s),
                    Child, s->balance[1] = BalanceObject, End,
                    Child, s->slot[MUI_SLOT_DETAIL] = mui_detail_group(s),
                End,
                Child, s->slot[MUI_SLOT_PLAYER] = mui_player_group(s),
            End, End, End;
    if (s->app == NULL) {
        if (strip != NULL) {
            MUI_DisposeObject(strip);
        }
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
        mui_report(MUI_FAIL_NO_WINDOW, "");
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
        mui_report(MUI_FAIL_NO_WINDOW, "");
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

static void mui_report(mui_fail why, const char* detail) {
    struct EasyStruct es;
    char text[300];
    snprintf(text, sizeof text, "%s %s", mui_fail_text(why), detail);
    fprintf(stderr, "%s\n", text);
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE*)"Selecta";
    es.es_TextFormat = (UBYTE*)"%s";
    es.es_GadgetFormat = (UBYTE*)"Continue";
    EasyRequest(NULL, &es, NULL, (ULONG)text);
}
#else
static void mui_report(mui_fail why, const char* detail) {
    fprintf(stderr, "%s %s\n", mui_fail_text(why), detail);
}

static void* mui_open(const sel_ui_open_args* a) {
    (void)a;
    mui_report(mui_present() ? MUI_FAIL_NO_WINDOW : MUI_FAIL_NO_LIBRARY, "");
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
    {
        uint32_t k, off = 0;
        s->p4_stat[0] = '\0';
        for (k = 0; k < P4_COUNT && off < sizeof s->p4_stat; k++) {
            off += (uint32_t)snprintf(s->p4_stat + off, sizeof s->p4_stat - off, "%s%s=%u", k ? " " : "", p4_name[k], (unsigned)s->p4[k]);
        }
    }
    s->stats[6].tag = SEL_UI_STAT_MUI_P4;
    s->stats[6].value = (uintptr_t)s->p4_stat;
    s->stats[7].tag = SEL_TAG_END;
    s->stats[7].value = 0;
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
