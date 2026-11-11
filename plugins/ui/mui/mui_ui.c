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
#include <time.h>

#include "map/mui_map.h"
#include "map/mui_theme.h"
#include "map/mui_fontprobe.h"
#include "map/mui_pens.h"
#include "map/mui_iconmap.h"
#include "map/mui_icons.h"
#include "map/mui_route.h"
#include <selecta/uimodel/about.h>
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/libfilter.h>
#include <selecta/uimodel/briefs.h>
#include <selecta/uimodel/pages.h>
#include <selecta/uimodel/theme.h>
#include <selecta/uimodel/viewcmds.h>
#include <selecta/tags.h>
#include <selecta/library_item.h>

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
#include "mui_gfxbase.h"
#include <graphics/displayinfo.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <proto/muimaster.h>
/* I972: diskfont.library loads the screen font; the base is renamed for the same reason as GfxBase (mui_gfxbase.h, I719):
 * libnix would open it at program start in selecta-cli too. Opened and closed around the own screen only. */
#define DiskfontBase sel_mui_DiskfontBase
#include <graphics/text.h>
#include <proto/diskfont.h>
struct Library* DiskfontBase;
struct Library* MUIMasterBase;
/* I719: graphics.library, opened by mui_open (see mui_gfxbase.h); the base symbol is renamed so libnix does not
 * auto-open it at program start. */
struct GfxBase* GfxBase;

static void mui_libs_close(void) {
    if (GfxBase != NULL) {
        CloseLibrary((struct Library*)GfxBase);
        GfxBase = NULL;
    }
    if (MUIMasterBase != NULL) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
    }
}
#include "mui_aabtn_class.h"
#include "mui_aapanel_class.h"
#include "mui_aatabs_class.h"
#include "mui_aabalance_class.h"
#include "mui_aaslider_class.h"
#include "mui_aacycle_class.h"
#include "mui_aalist_class.h"
#include "mui_aafield_class.h"
#include "mui_aatext_class.h"
#include "mui_text.h"
#include "mui_aawrap.h"
#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include "map/mui_aa.h"
#include "map/mui_log.h"
#include "mui_canvas_class.h"
#include "mui_vizarea_class.h"
#include "mui_listview_class.h"
#include "mui_keys_class.h"
#include <selecta/uimodel/viewcmds.h>
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
#define MUI_STATS_MAX 24u /* rows of the Statistics window; the engine's stats buffer holds far fewer */
#define CTL_STATS_MORE 0x280AL /* I1181: the Statistics window's More / Less button */
#define CTL_OFFER_NOTNOW 0x280BL /* I1182: the first-start sync offer, Not now */
#define CTL_OFFER_SYNC 0x280CL   /* its Sync now button */
#define CTL_OFFER_NEVER 0x280DL  /* I1308c: its Never button */
#define CTL_OFFER_ROW 0x2900L    /* its per-source checks, CTL_OFFER_ROW + k */
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
       P4_CLEARLIB, P4_PNEW, P4_HELP, P4_HOME, P4_TOAST, P4_CRUMB, P4_STATE, P4_SETTINGS, P4_SONGMENU, P4_NP, P4_SONGINFO, P4_PREF, P4_TRACKER,
       P4_VIZPICK, P4_VIZFRAMES, P4_DBLRACE, P4_NPPAGE, P4_PFOLDER, P4_SIWRONG, P4_SISKIP, P4_SINOTE, P4_COUNT };
static const char* const p4_name[P4_COUNT] = {"search", "cmd", "like", "rating", "subsong", "qremove", "qclear", "qup", "qdown",
                                              "popen", "prename", "pdelete", "clearlib", "pnew", "help", "home", "toast", "crumb", "state", "settings",
                                              "songmenu", "np", "songinfo", "pref", "tracker", "vizpick", "vizframes", "dbl_race_injected", "nppage", "pfolder", "siwrong", "siskip", "sinote"};
#define MUI_PREF_MAX SEL_UIM_PREF_ROWS_MAX /* MUI-P4-10: Settings row capacity; the table is checked against it at compile time (MPS-1b) */
#define MUI_INFO_MAX 24u   /* MUI-P4-9: most song info rows */
#define MUI_TRK_ROWS 16u   /* MUI-P4-16: pattern rows shown */
#define MUI_OFF_ROWS 6u    /* I1313d: most "Players Turned Off" rows */
#define MUI_TRK_CHANNELS 4u /* MUI-P4-16: channel columns shown */

typedef struct mui_shelf_ctx { /* I795: the row callback of a shelf list finds its mui_state and slot here */
    struct mui_state* s;
    uint32_t slot;
} mui_shelf_ctx;

/* I798 (O3): one persistent ShortHelp buffer per tipped control. */
enum { MUI_TS_BTN = 0, MUI_TS_DOOR = MUI_BTN_COUNT, MUI_TS_LIKE = MUI_TS_DOOR + MUI_SIDEBAR_DOORS, MUI_TS_NP, MUI_TS_SEARCH, MUI_TS_CMD, MUI_TS_PAL, MUI_TS_SEEK, MUI_TS_VOL, MUI_TS_COUNT };

typedef struct mui_state {
    uint32_t opens; /* ui_open_mui sentinel (MUI-P1-5): windows opened */
    uint32_t wake_mask; /* engine wake signal (MUI-P1-7), 0 where there is none */
    uint32_t menu_sends; /* menu items that sent their action line to the engine (MUI-P2-4) */
    sel_engine* engine;
    uint32_t transport_hits; /* transport buttons and sliders that sent their line (MUI-P2-5): stat transport_hits */
    uint32_t row_plays;      /* list rows activated by double click (MUI-P3-1): stat row_plays */
    int dbl_race;            /* I724: ENV:Selecta/MUIDblRace was set at open: every double click first gets a forced list refresh (rig knob) */
    uint32_t drops;          /* AppWindow arguments sent to the engine (MUI-P5-6): stat drops */
    sel_tag stats[8];
    char p4_stat[480];       /* MUI-P4: the sentinel printed as ui_p4: how often each P4 gadget sent its line */
    uint32_t p4[P4_COUNT];
    sel_uim uim;             /* the frame model the controls are driven from (MUI-P2-6) */
    char list_stat[480];     /* the list sentinel printed as ui_list: back end, style, columns, rows (native List only) */
#ifdef SEL_MUI_NATIVE
    Object* app;
    struct MsgPort* tport;          /* timer.device reply port: bounds the native wait by timeout_ms */
    struct timerequest* treq;       /* NULL when timer.device failed: the wait then falls back to Delay(1) */
    Object* win;
    Object* root;                   /* window contents: its MUIA_Width drives the breakpoint (MUI-P2-2) */
    Object* slot[MUI_SLOT_COUNT];   /* the five layout regions and the top bar, created once, shown or hidden */
    Object* balance[2];             /* sidebar|main and main|detail */
    uint32_t open_layout_w; /* I973h: layout width of the ladder step that opened the window (cap for the resize check) */
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
    mui_route route;                /* I794: the shared route history (T1) and the handler that enters the pages (P2, P3) */
    Object* door_btn[MUI_SIDEBAR_DOORS]; /* I797 (S2): the sidebar door buttons, lit from the route history in mui_route_sync */
    mui_palette pal;                /* I798 (O1): the command palette model and its window */
    Object *pal_win, *pal_str, *pal_list, *np_btn, *pal_btn;
    char tip[MUI_TS_COUNT][96];     /* I798 (O3): the ShortHelp texts, kept alive for MUI */
    Object* nav_back;               /* I794: top bar Back / Forward, disabled from the route history */
    Object* nav_fwd;
    Object* col_page;               /* I794 (P3): album, artist or composer page: header, meta line and the song AAList; replaces the main list while shown */
    Object* col_list;
    Object* col_artist;             /* the Artist link of an album page (META_ARTIST) */
    Object* col_title;
    Object* col_meta;
    char col_title_buf[160];
    char col_meta_buf[224];
    uint32_t col_serial;            /* the collection serial the list shows */
    sel_uim_briefs col_brf;         /* I1306b: the catalogued song titles of the page, one posted song_briefs read per page (shared model) */
    int col_shown;                  /* 1 while col_page replaces the list */
    Object* list;
    mui_list_entry entries[MUI_ROWS_MAX];
    const char* entry_ptr[MUI_ROWS_MAX + 1];
    mui_cells cell[MUI_ROWS_MAX];   /* MUI-P3-4: the List entries; the display hook reads the cells from them */
    mui_aalist_cols main_cols;       /* I667i2: weights and fixed-width samples of the shown columns */
    uint32_t style_kind, style_cols, tile_step; /* I1174: how the library style is shown (MUI_STYLE_*), Grid cells per row, the tile size step */
    int32_t style_cursor;            /* I1174: window position of the cursor row before packing */
    uint32_t cell_n;                 /* I1079d: how many of cell[] hold the rows now listed (the absent-column rule reads them) */
    char lyric_line[256];            /* I667i2: the lyrics line handed to the lyrics AAList */
    char col_fmt[128];               /* the shown column set as MUI format text (mui_columns_format), the rig read-back */
    uint32_t col_bp;                /* the breakpoint col_fmt was built for */
    Object* str_search;             /* MUI-P4-3 */
    Object* keys;                   /* I1171: Selecta_Keys, the window key handler */
    Object* str_cmd;                /* MUI-P4-13 */
    Object* cyc_rating;             /* MUI-P4-7 */
    Object* cyc_subsong;            /* MUI-P4-9 */
    Object* sub_grp;                /* I973c: the bar's subsong group (label + minus + plus), shown only when the song has subsongs */
    Object* sub_txt;                /* I973c: its "n/m" label */
    char sub_buf[16];
    uint32_t sub_shown;
    Object* status;                 /* the last line a P4 gadget sent (MUI-P4-11: the one-line toast) */
    mui_toast toast;                /* MUI-P4-11 */
    Object* help_win;               /* MUI-P4-14: created on the first Help click */
    Object* stats_win;              /* I1181: the Library Statistics window, created on the first stats_show */
    Object* stats_more;             /* its More / Less button */
    Object* stats_lbl[MUI_STATS_MAX];
    Object* stats_val[MUI_STATS_MAX];
    mui_stats_panel stats_panel;
    Object* offer_win;              /* I1182: the first-start sync offer, built when it opens */
    Object* offer_row[SEL_SOURCE_MAX];
    mui_offer_panel offer;
    int offer_asked;                /* SOURCES was requested once for the offer */
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
    Object* np_note;                /* I1127c: "Sample bank not found" under the album line */
    Object* np_tabs;                /* I960d: the Now Playing page's tab strip (Lyrics, Info, Tracks) */
    Object* viz_area;               /* I684: the Now Playing visualizer (Selecta_VizArea) and its plugin picker */
    Object* cyc_viz;
    mui_mainpane pane;              /* I735: the main-slot pane shown (list, Visualizer page, Pattern page) and its opening counters */
    Object* main_pages;             /* the main slot: a page group of the list, the Visualizer page and the Pattern page */
    Object* viz_page_area;          /* the Visualizer page: its own Selecta_VizArea over the same vizset (stepped only while shown) */
    Object* cyc_viz_page;
    Object* tabs_viz_page;          /* I918: the Visualizer page chooser as tabs (shown when every full name fits), else cyc_viz_page is shown */
    int viz_tabs_on;                /* 1 tabs shown, 0 cycle shown, -1 not decided yet */
    Object* trk_page_list;          /* the Pattern page rows, over the same trk_ptr as the detail tab */
    mui_vizset vizset;
    mui_find_plugin_fn find_plugin;
    Object* np_lyrics;
    Object* np_up[3];
    char np_buf[4][112];
    char np_lyric_buf[1024];
    char np_up_buf[3][96];
    Object* np_off_box;             /* I1313d: "Players Turned Off" heading + rows, shown only when the machine filter hid a player */
    Object* np_off[MUI_OFF_ROWS];
    char np_off_buf[MUI_OFF_ROWS][128];
    Object* info_list;              /* MUI-P4-9 song info rows */
    volatile uint32_t mode_pending; /* I1173: 1 + the SEL_UIM_LAYOUT_* a ui.desktop.mode change asked for; the event runs on the engine's pump, the window is touched by mui_refresh */
    uint32_t lib_filter;            /* I670: the library filter in force (sel_uim_lfilter_*), 0 All */
    int side_rail;                  /* I949c: the sidebar is the compact door rail, side_dyn stays NULL */
    Object* side_expand_btn;        /* I1166b: the Expand / Collapse Library button, its chevron turns with the layout */
    Object* side_dyn;               /* I669: the sidebar's S5 to S9 group, rebuilt from the model when the filter or the width moves */
    int side_top_bar, side_single;  /* the top bar flag of the build; the chips-or-label decision the dyn group was built with */
    sel_engine_library_item side_items[MUI_SIDEBAR_ROWS]; /* the rows the shared filter pass (sel_uim_lfilter_pass) returned for lib_filter */
    uint32_t side_item_n;
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
    mui_flag crumb_show, nav_back_dis, nav_fwd_dis; /* I966: last ShowMe / Disabled written, so mui_refresh sets only on a change */
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
    Object* home_page;              /* I795 (P1): quick tile row and shelves; replaces the main list on Home and on a Show all page */
    Object* quick_row;
    Object* quick_btn[MUI_HOME_QUICK_MAX];
    Object* shelf_grp[MUI_HOME_SHELF_MAX];
    Object* shelf_head[MUI_HOME_SHELF_MAX];
    Object* shelf_all[MUI_HOME_SHELF_MAX];
    Object* shelf_list[MUI_HOME_SHELF_MAX];
    char shelf_head_buf[MUI_HOME_SHELF_MAX][96];
    mui_shelf_ctx shelf_ctx[MUI_HOME_SHELF_MAX];
    uint32_t shelf_first[MUI_HOME_SHELF_MAX], shelf_id[MUI_HOME_SHELF_MAX]; /* window row of the first entry the slot lists, the shelf it shows */
    mui_home_page home;
    uint32_t home_sig;
    Object* browse_page;            /* I795 (P4): the Browse tile grid */
    Object* tile_btn[MUI_TILE_MAX];
    mui_btile tile[MUI_TILE_MAX];
    uint32_t tile_n, tile_sig;
    Object* facet_row;              /* I795 (P5): the Search facet chips above the results */
    Object* facet_btn[SEL_UIM_SEARCH_FACETS];
    uint32_t search_facet;
    int home_shown, browse_shown, facet_shown, modes_shown, scope_shown;
    Object* scope_row;              /* I1176: the Queue scope Cycle (Library, Album, Playlist) above the queue list */
    Object* cyc_scope;
    const char* scope_ent[MUI_SCOPE_COUNT + 1u];
    uint32_t scope_seen;            /* the scope index the Cycle shows, from st->options[SEL_OPT_SCOPE] */
    mui_srcs srcs;                  /* I1175: the Sources page rows (map/mui_sources.c), filled by mui_on_event on the engine pump */
    Object* src_page;               /* I1175: the Sources page: row list, Switch and Sync */
    Object* src_list;
    Object* src_switch;
    Object* src_sync;
    Object* mode_row;               /* I1175: the Library mode Cycle above the list */
    Object* cyc_libmode;
    const char* libmode_ent[MUI_LIBMODE_COUNT + 1u];
    int src_shown;
    uint32_t src_gen_seen;
    long src_pick;
    char src_rowbuf[128];
#endif
} mui_state;

static mui_state g_state;
static sel_tag g_hooks[10];
static sel_tag g_desc[5];

#define MUI_THEME_ENV_PATH "ENV:Selecta/theme"

/* I676b/I676c: SEL_UI_THEME hook, called by the ARexx command THEME. 0: the file is valid, remembered for the next open and applied
 * to the open UI now (mui_theme_live_now: palette and AA classes repaint in place, no window close or open). Non-zero: refused,
 * nothing changed. */
static char g_theme_arg[256]; /* the launch --theme path, kept so a live THEME resolves the same way the open did */
static int mui_theme_live_now(void);
static int mui_theme_hook(void* state, const char* path) {
    (void)state;
    if (mui_theme_request(MUI_THEME_ENV_PATH, path) != 0) {
        return -1;
    }
    return mui_theme_live_now();
}

static void put_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

#ifdef SEL_MUI_NATIVE
static uint32_t mui_now_ms(void);
/* The tool button order is the label order of mui_tool_labels (map/mui_layout.c, tested on the host): this lists each button's id. */
static const uint32_t g_tool_which[MUI_TOOL_BTNS] = {P4_LIKE, P4_QREMOVE, P4_QCLEAR, P4_QUP, P4_QDOWN, P4_HELP, P4_SETTINGS, P4_PNEW, P4_PFOLDER, P4_POPEN, P4_PRENAME, P4_PDELETE, P4_CLEARLIB};
/* MUI-P2-3: the Menustrip built from the action table (mui_menu_build). MUI keeps the title and shortcut pointers, so the
 * rows and the one-character shortcut strings are static. A Menuitem trigger is a ReturnID (MENU_ID_BASE + row) that the
 * wait loop turns into the row's protocol line for the engine (MUI-P2-4). */
#define MENU_MAX 160
#define MENU_ID_BASE 0x1000L
#define CTL_KEY_BASE 0x2B00L /* I1171: window action keys (Space, Enter, Home, End, M-*, C-*), CTL_KEY_BASE + mui_key_row index */
#define CTL_SM_BASE 0x2500L /* MUI-P4-6: Song menu items, CTL_SM_BASE + item */
#define CTL_PREF_BASE 0x2600L /* MUI-P4-10: Settings gadgets, CTL_PREF_BASE + pref row */
#define CTL_CLIP_COPY 0x2120L /* Edit > Copy Song Title (MUI-P5-6), outside the MENU_ID_BASE + row range */
#define CTL_PANE_BASE 0x2130L /* I735: View > Visualizer Page, Pattern Page: CTL_PANE_BASE + mui_pane (MUI_PANE_VIZ, MUI_PANE_TRACKER) */
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
        Object* it = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG)mui_l1_keep(sel_uim_song_menu_label(i, 0)),
                                   MUIA_Menuitem_Shortcut, (ULONG)keys[i], TAG_DONE);
        g_sm_item[which][i] = it;
        if (it != NULL) {
            DoMethod(parent, OM_ADDMEMBER, it);
            DoMethod(it, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
                     (ULONG)(CTL_SM_BASE + (LONG)i));
        }
    }
}

/* I1179 (I799 C1, C2): the Listview's context strip. The five song items (item 4 reads Remove from Queue on a queue entry), then the
 * playlist items Rename and Delete (sel_uim_plmenu_label), firing the P4_PRENAME and P4_PDELETE handlers the tool buttons use. The strip
 * is built once; mui_ctx_sync enables what the cursor row offers (sel_uim_ctx_mask) and swaps item 4's title. Remove from Playlist
 * (SEL_UIM_CTX_PLREMOVE) has no row to act on: the MUI lists no playlist's songs (a playlist row plays on activate), so the item is
 * exempt until a playlist page exists; its line and label are the shared sel_uim_plremove_* the desktop uses. */
#define CTL_P4_BASE_CTX 0x2300L /* same value as CTL_P4_BASE below */
static Object* g_ctx_pl[2];
static Object* mui_song_context_menu(void) {
    Object* strip = MUI_NewObject((char*)MUIC_Menustrip, TAG_DONE);
    Object* menu = MUI_NewObject((char*)MUIC_Menu, MUIA_Menu_Title, (ULONG) "Song", TAG_DONE);
    uint32_t k;
    if (strip == NULL || menu == NULL) {
        return NULL;
    }
    DoMethod(strip, OM_ADDMEMBER, menu);
    mui_song_menu_items(menu, 1u);
    for (k = 0; k < 2u; k++) {
        uint32_t act = sel_uim_plmenu_action(0u, k);
        Object* it = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG)sel_uim_plmenu_label(act), MUIA_Menuitem_Enabled,
                                   (ULONG)FALSE, TAG_DONE);
        g_ctx_pl[k] = it;
        if (it != NULL) {
            DoMethod(menu, OM_ADDMEMBER, it);
            DoMethod(it, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
                     (ULONG)(CTL_P4_BASE_CTX + (LONG)(act == SEL_UIM_PLACT_RENAME ? P4_PRENAME : P4_PDELETE)));
        }
    }
    return strip;
}

static int mui_screen_compact(void); /* I949c, defined with the screen helpers; the View menu needs it (I960d) */
/* I1179: the cursor row decides which context items work. Called after every list refresh. */
static void mui_ctx_sync(const sel_view_state* view) {
    uint32_t kind = SEL_UIM_CTXK_NONE, mask, i, w;
    if (view != NULL && view->cursor >= view->window_start && view->cursor - view->window_start < view->row_n) {
        kind = mui_ctx_kind((uint32_t)view->rows[view->cursor - view->window_start].kind);
    }
    mask = sel_uim_ctx_mask(kind);
    for (w = 0; w < 2u; w++) {
        for (i = 0; i < SEL_UIM_SONG_MENU_ITEMS; i++) {
            if (g_sm_item[w][i] != NULL) {
                SetAttrs(g_sm_item[w][i], MUIA_Menuitem_Enabled, (ULONG)(((mask >> i) & 1u) ? TRUE : FALSE), TAG_DONE);
                if (i == 4u) {
                    SetAttrs(g_sm_item[w][i], MUIA_Menuitem_Title, (ULONG)sel_uim_song_menu_label(4u, kind == SEL_UIM_CTXK_QUEUE), TAG_DONE);
                }
            }
        }
    }
    for (i = 0; i < 2u; i++) {
        if (g_ctx_pl[i] != NULL) {
            SetAttrs(g_ctx_pl[i], MUIA_Menuitem_Enabled,
                     (ULONG)((mask & (i == 0u ? SEL_UIM_CTX_RENAME : SEL_UIM_CTX_DELETE)) ? TRUE : FALSE), TAG_DONE);
        }
    }
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
                      cls_is_menu ? MUIA_Menu_Title : MUIA_Menuitem_Title, (ULONG)mui_l1_keep(title), TAG_DONE);
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
        item = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG)mui_l1_keep(m->item),
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
        /* I735: the Visualizer and Pattern pages open from View, with the Mac UI's keys (mui_pane_shortcut) as shortcuts. */
        static char pane_sc[MUI_PANE_COUNT][2];
        Object* view = menu_child(strip, 1, "View", NULL, t_name, t_obj, &nt, "View");
        uint32_t p;
        for (p = MUI_PANE_LIST + 1u; view != NULL && p < MUI_PANE_COUNT; p++) {
            Object* pi;
            if (p == MUI_PANE_NOWPLAYING && !mui_screen_compact()) {
                continue; /* I960d: the Now Playing page exists in the compact class only; the full class has the detail column */
            }
            pane_sc[p][0] = mui_pane_shortcut(p);
            pane_sc[p][1] = '\0';
            pi = MUI_NewObject((char*)MUIC_Menuitem, MUIA_Menuitem_Title, (ULONG)mui_l1_keep(mui_pane_title(p)), MUIA_Menuitem_Shortcut,
                               (ULONG)(pane_sc[p][0] != '\0' ? pane_sc[p] : NULL), TAG_DONE);
            if (pi != NULL) {
                DoMethod(view, OM_ADDMEMBER, pi);
                DoMethod(pi, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Application, 2,
                         MUIM_Application_ReturnID, (ULONG)(CTL_PANE_BASE + (LONG)p));
            }
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

/* Sends one command line. The pool is decode scratch only: the engine copies string arguments into its command slot on
 * send (I1082b; before that a stack pool left "VIEW_SOURCE search" with an empty source, I569). */
static int mui_engine_line(mui_state* s, const char* line) {
    char pool[512];
    int ok = line != NULL && s->engine != NULL && sel_engine_send_line(s->engine, line, pool, sizeof pool) == SEL_CODEC_OK;
    if (line != NULL && !ok) {
        fprintf(stderr, "selecta mui: engine refused line \"%s\"\n", line); /* I791: a refused line is not silent */
    }
    return ok;
}

static void mui_stats_window(mui_state* s);
static void mui_stats_sync(mui_state* s);
static void mui_offer_window(mui_state* s);
static int mui_route_send(void* ctx, const char* line);

/* A fired Menuitem: the same line the desktop sends for the action id (MUI-P2-4). */
static void mui_menu_fire(mui_state* s, uint32_t row) {
    const char* line = row < g_menu_n ? mui_menu_line(g_menu[row].id) : NULL;
    if (mui_engine_line(s, line)) {
        s->menu_sends++;
        if (mui_stats_menu(&s->stats_panel, g_menu[row].id)) {
            mui_stats_window(s);
        }
    }
}

/* MUI-P2-5/P2-6/P3-1: the controls. ReturnIDs: buttons, sliders and list events are turned into engine lines in
 * mui_control_fire, the same way a Menuitem is (mui_menu_fire). */
#define CTL_BTN_BASE 0x2000L
#define CTL_SEEK 0x2100L
#define CTL_VOLUME 0x2101L
#define CTL_LIST_DBL 0x2110L
#define CTL_LIST_SEL 0x2111L
#define CTL_NAV_BACK 0x2801L   /* I794: top bar Back */
#define CTL_NAV_FWD 0x2802L    /* I794: top bar Forward */
#define CTL_COL_LIST 0x2803L   /* I794: a song of the collection page was picked */
#define CTL_COL_ARTIST 0x2804L /* I794: the Artist link of an album page */
#define CTL_NP_TOGGLE 0x2805L  /* I798 (T2, R3): Hide / Show Now Playing at the right end of the top bar */
#define CTL_PAL_BTN 0x2806L    /* I798 (O1): the Commands button opening the palette window */
#define CTL_PAL_STR 0x2807L    /* the palette window's query String */
#define CTL_PAL_LIST 0x2808L   /* the palette window's match list */
#define CTL_DLG_CLOSE 0x2809L  /* I730: the close button of a dialog header (the window closes itself; the id is a no-op) */
#define CTL_TOPBAR_BROWSE 0x2800L /* I615d: the top bar Browse button */
#define CTL_QUICK_BASE 0x2A00L      /* I795 (P1): the Home quick tiles, CTL_QUICK_BASE + slot */
#define CTL_SHELF_ALL_BASE 0x2A10L  /* I795 (P1): Show all of a shelf slot */
#define CTL_SHELF_LIST_BASE 0x2A20L /* I795 (P1): a row of a shelf slot was picked */
#define CTL_TILE_BASE 0x2A40L       /* I795 (P4): the Browse tiles, CTL_TILE_BASE + slot */
#define CTL_FACET_BASE 0x2A80L      /* I795 (P5): the Search facet chips, CTL_FACET_BASE + facet */
#define CTL_LIBMODE 0x2AC0L         /* I1175: the Library mode Cycle (Composers, Tree, Columns, Files, History, Sources) */
#define CTL_SRC_LIST 0x2AC1L        /* I1175: a row of the Sources page was picked */
#define CTL_SRC_SWITCH 0x2AC2L      /* I1175: the Sources page Switch button (the per-source toggle) */
#define CTL_SCOPE 0x2AC4L           /* I1176: the Queue scope Cycle */
#define CTL_SRC_SYNC 0x2AC3L        /* I1175: the Sources page Sync button */
#define CTL_DOOR_BASE 0x2200L /* MUI-P4-0: one button per sidebar door */
#define MUI_DOOR_MAX 8u
#define CTL_SIDE_BASE 0x2900L /* I793: the Your Library sidebar presses, CTL_SIDE_BASE + MUI_SIDE_* */
enum { MUI_SIDE_TITLE = 0, MUI_SIDE_CREATE, MUI_SIDE_EXPAND, MUI_SIDE_CARD, MUI_SIDE_CHIP, MUI_SIDE_COUNT = MUI_SIDE_CHIP + 6 };
#define CTL_CRUMB_BASE 0x2400L /* MUI-P4-2: one button per breadcrumb piece */
#define CTL_P4_BASE 0x2300L /* MUI-P4: CTL_P4_BASE + P4_* */
static const char* const p4_rating_entries[] = {"None", "1 star", "2 stars", "3 stars", "4 stars", "5 stars", NULL};
static const char* const p4_subsong_entries[] = {"Subsong 1", "Subsong 2", "Subsong 3", "Subsong 4", "Subsong 5", "Subsong 6", "Subsong 7", "Subsong 8", NULL};
#define CTL_LIB_PREV 0x2810L /* I670: the sidebar library filter, previous and next */
#define CTL_LIB_NEXT 0x2811L
#define CTL_SUB_PREV 0x2812L /* I973c: the bar's subsong minus and plus */
#define CTL_SUB_NEXT 0x2813L
#define CTL_EXTRA_BASE 0x2700L /* MUI-P8-1: album and speed buttons of the transport area */



/* I648: the Selecta look. g_look is filled once at open from the txt_theme tokens (the desktop UI's table); an entry with
 * on == 0 leaves the user's MUI background and frame alone. TH_TAGS expands to two tag pairs: MUIA_Background and
 * MUIA_Frame, each TAG_IGNORE when the look is off, so the same call works either way. */
static struct {
    mui_theme_look look;
    int on;
} g_th[MUI_TH_COUNT];
static const uint32_t* g_tokens; /* I667e: the theme tokens for the panel faces; = mui_theme_tokens(), set in mui_theme_init */
static uint32_t g_text_rgb; /* I665: the TEXT token, caption colour of the flat button */

#define TH_TAGS(role, frame_off) \
    g_th[role].on ? (ULONG)MUIA_Background : (ULONG)TAG_IGNORE, g_th[role].on ? (ULONG)g_th[role].look.background : 0UL, \
    (ULONG)MUIA_Frame, g_th[role].on ? (ULONG)g_th[role].look.frame : (ULONG)(frame_off),
#define TH_BG_OR(role, spec) MUIA_Background, g_th[role].on ? (ULONG)g_th[role].look.background : (ULONG)(spec),

/* I666: Selecta's own public screen. MUI maps every colour it is asked for to the nearest pen the screen already has; the
 * Workbench screen (4 colours on the RTG rig, 16 on AGA) holds no dark greys and no soft grey, so surfaces went black and text
 * white. mui_pens_plan_for decides the palette and DrawInfo pens from the desktop UI tokens; here the screen opens with exactly
 * that palette (SA_Colors32, SA_Pens) and every token colour is obtained at exact precision and held, so MUI's own pen requests
 * for the same RGB find them. Native planar modes get 16 colours (chip RAM bandwidth), RTG modes 256. If the screen cannot open,
 * the windows stay on the Workbench screen and the look degrades as before (g_scr stays NULL). */
#define MUI_PUB_NAME "SELECTA"
static struct Screen* g_scr;
static mui_pens_plan g_plan;
static LONG g_pen_held[MUI_PEN_TOTAL];
static int g_pixel_aspect_q8 = 256; /* I949a: pixel height over width x 256 of the opened screen mode, read once in mui_screen_open */
static int g_screen_rtg; /* I684b: the Workbench mode is a foreign (RTG) one; the own screen keeps that kind, planar stays planar */

/* I972: the own screen's font follows the tier (mui_aa_screen_font). MUI's stock objects and the PLAIN AA backend (no
 * ttengine or no Inter) draw in the screen font; without SA_Font they kept the 8 px Workbench font at 800x600. */
static struct TextFont* g_scr_font;
static struct TextAttr g_scr_font_attr;
/* I972c: MUI 3.8 takes its Normal font from MUI prefs, not the screen font, so every label stayed 9 px at 800x600. The root group of each
 * window gets MUIA_Font = the tier TextFont (children inherit); g_scr_font stays open until mui_screen_close, after the app is disposed.
 * No tier font (256-row tier, font missing): TAG_IGNORE, the user's MUI font is untouched. g_font_applied counts the roots that got it. */
static int g_font_applied;
static ULONG mui_tier_font_tag(void) {
    if (g_scr_font == NULL) {
        return (ULONG)TAG_IGNORE;
    }
    g_font_applied++;
    return (ULONG)MUIA_Font;
}
#define TH_FONT mui_tier_font_tag(), (ULONG)g_scr_font,

static void mui_screen_font_close(void) {
    if (g_scr_font != NULL) {
        CloseFont(g_scr_font);
        g_scr_font = NULL;
    }
    if (DiskfontBase != NULL) {
        CloseLibrary(DiskfontBase);
        DiskfontBase = NULL;
    }
}

/* Opens the tier font for a screen of rows x cols; NULL keeps the Workbench font (256-row tier, or the font is missing). */
static struct TextAttr* mui_screen_font_open(int rows, int cols) {
    mui_aa_screen_font_t f;
    if (!mui_aa_screen_font(mui_aa_font_px(rows, cols), &f)) {
        return NULL;
    }
    DiskfontBase = OpenLibrary((CONST_STRPTR) "diskfont.library", 36);
    if (DiskfontBase == NULL) {
        fprintf(stderr, "selecta mui: diskfont.library missing, screen keeps the Workbench font\n");
        return NULL;
    }
    g_scr_font_attr.ta_Name = (STRPTR)f.name;
    g_scr_font_attr.ta_YSize = (UWORD)f.ysize;
    g_scr_font_attr.ta_Style = FS_NORMAL;
    g_scr_font_attr.ta_Flags = 0;
    g_scr_font = OpenDiskFont(&g_scr_font_attr);
    if (g_scr_font == NULL) {
        fprintf(stderr, "selecta mui: %s %d missing, screen keeps the Workbench font\n", f.name, f.ysize);
        mui_screen_font_close();
        return NULL;
    }
    g_scr_font_attr.ta_YSize = g_scr_font->tf_YSize; /* the size diskfont found */
    fprintf(stderr, "selecta mui: screen font %s %d\n", f.name, (int)g_scr_font->tf_YSize);
    return &g_scr_font_attr;
}

static void mui_screen_close(void) {
    int i;
    aatip_drop(NULL); /* I731: the tip window lives on the Selecta screen */
    if (g_scr == NULL) {
        return;
    }
    for (i = 0; i < MUI_PEN_TOTAL; i++) {
        if (g_pen_held[i] >= 0) {
            ReleasePen(g_scr->ViewPort.ColorMap, (ULONG)g_pen_held[i]);
            g_pen_held[i] = -1;
        }
    }
    PubScreenStatus(g_scr, PSNF_PRIVATE);
    if (!CloseScreen(g_scr)) {
        PubScreenStatus(g_scr, 0); /* visitor windows still open: leave the screen public rather than pull it away */
        fprintf(stderr, "selecta mui: screen %s still has visitor windows, left open\n", MUI_PUB_NAME);
        g_scr = NULL;
        return; /* the screen still uses g_scr_font: keep it and diskfont open */
    }
    g_scr = NULL;
    mui_screen_font_close();
}

/* I625a5: sub-phase lines of the UI open phase on the shell's trace clock (sel_ui_open_args.trace), so the --log shows where the
 * seconds inside "UI open returned" go. Silent when the shell gave no trace function or the run has neither --log nor --stats. */
static const sel_host* g_trace_host;
static void (*g_trace_fn)(const sel_host*, const char*);
static void mui_trace(const char* phase) {
    if (g_trace_fn != NULL) {
        g_trace_fn(g_trace_host, phase);
    }
}

static void mui_screen_open(const uint32_t* tokens) {
    struct Screen* wb;
    struct DisplayInfo di;
    ULONG mode = INVALID_ID;
    ULONG wb_w = 0, wb_h = 0;
    int foreign = 0;
    mui_screen_plan_t plan;
    struct TextAttr* font;
    ULONG pal[2 + MUI_PEN_TOTAL * 3];
    int i;
    for (i = 0; i < MUI_PEN_TOTAL; i++) {
        g_pen_held[i] = -1;
    }
    wb = LockPubScreen(NULL);
    if (wb != NULL) {
        mode = GetVPModeID(&wb->ViewPort);
        wb_w = (ULONG)wb->Width;
        wb_h = (ULONG)wb->Height;
        memset(&di, 0, sizeof di);
        /* I667n: uaegfx (Picasso96) modes leave DIPF_IS_FOREIGN clear, so the planar 16-pen plan ran on RTG; the bitmap decides too. */
        foreign = mui_screen_is_rtg(mode != (ULONG)INVALID_ID && GetDisplayInfoData(NULL, (UBYTE*)&di, sizeof di, DTAG_DISP, mode) > 0 &&
                                        (di.PropertyFlags & DIPF_IS_FOREIGN) != 0,
                                    (GetBitMapAttr(wb->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD) != 0,
                                    (uint32_t)GetBitMapAttr(wb->RastPort.BitMap, BMA_DEPTH));
        UnlockPubScreen(NULL, wb);
    }
    g_screen_rtg = foreign;
    if (!mui_pens_plan_for(tokens, &g_plan)) {
        return;
    }
    /* I671: MUI 3.8 refuses a window taller than its screen, and the layout is taller than a 640x256 AGA Workbench. The plan
     * (map/mui_screen.c) keeps the Workbench mode when the layout fits and takes its interlaced variant (height doubled) when
     * it does not; a laced mode the chipset cannot show keeps the Workbench mode and the window failure says why. */
    mui_screen_plan((uint32_t)mode, (uint32_t)wb_w, (uint32_t)wb_h, foreign, &plan);
    fprintf(stderr, "selecta mui: screen plan mode %08lx %lux%lu depth %lu\n", (unsigned long)plan.mode, (unsigned long)plan.width,
            (unsigned long)plan.height, (unsigned long)plan.depth);
    mode = plan.mode;
    /* I949a: the one aspect source; consumers (I949e) read g_pixel_aspect_q8 and never DisplayInfo. */
    g_pixel_aspect_q8 = 256;
    memset(&di, 0, sizeof di);
    if (mode != (ULONG)INVALID_ID && GetDisplayInfoData(NULL, (UBYTE*)&di, sizeof di, DTAG_DISP, mode) > 0) {
        g_pixel_aspect_q8 = mui_pixel_aspect_q8((int)di.Resolution.x, (int)di.Resolution.y);
    }
    fprintf(stderr, "selecta mui: pixel aspect %d/256 (resolution %dx%d)\n", g_pixel_aspect_q8, (int)di.Resolution.x, (int)di.Resolution.y);
    pal[0] = ((ULONG)MUI_PEN_TOTAL << 16) | 0u;
    for (i = 0; i < MUI_PEN_TOTAL; i++) {
        pal[1 + i * 3] = g_plan.rgb32[i][0];
        pal[2 + i * 3] = g_plan.rgb32[i][1];
        pal[3 + i * 3] = g_plan.rgb32[i][2];
    }
    pal[1 + MUI_PEN_TOTAL * 3] = 0;
    font = mui_screen_font_open((int)plan.height, (int)plan.width);
    g_scr = OpenScreenTags(NULL, SA_DisplayID, mode, SA_Overscan, OSCAN_TEXT, SA_Depth, (ULONG)plan.depth, SA_Colors32, (ULONG)pal, SA_Pens,
                           (ULONG)g_plan.dri_pens, SA_SharePens, TRUE, SA_PubName, (ULONG)MUI_PUB_NAME, SA_Title, (ULONG) "Selecta",
                           SA_ShowTitle, FALSE, SA_Type, PUBLICSCREEN, font != NULL ? SA_Font : TAG_IGNORE, (ULONG)font, TAG_DONE);
    if (g_scr == NULL) {
        fprintf(stderr, "selecta mui: own screen did not open, using the Workbench screen\n");
        mui_screen_font_close();
        return;
    }
    for (i = 0; i < MUI_PEN_TOTAL; i++) {
        LONG pen = ObtainBestPen(g_scr->ViewPort.ColorMap, g_plan.rgb32[i][0], g_plan.rgb32[i][1], g_plan.rgb32[i][2], OBP_Precision,
                                 PRECISION_EXACT, OBP_FailIfBad, TRUE, TAG_DONE);
        g_pen_held[i] = pen;
        if (pen < 0) {
            fprintf(stderr, "selecta mui: no exact pen for %06x\n", (unsigned)g_plan.rgb[i]);
        }
    }
    /* I685e1 rig check: the package reads the screen's DrawInfo resolution, Selecta its display database entry; this reports both once per open, no fallback. */
    {
        struct DrawInfo* dri = GetScreenDrawInfo(g_scr);
        int dri_q8 = 256;
        if (dri != NULL) {
            dri_q8 = mui_pixel_aspect_q8((int)dri->dri_Resolution.X, (int)dri->dri_Resolution.Y);
            FreeScreenDrawInfo(g_scr, dri);
        }
        fprintf(stderr, "mui aspect dri=%d displayinfo=%d mode=0x%08lx\n", dri_q8, g_pixel_aspect_q8, (unsigned long)GetVPModeID(&g_scr->ViewPort));
    }
    PubScreenStatus(g_scr, 0); /* from private to public: MUI windows find it by name */
}
#define TH_SCR (g_scr != NULL ? (ULONG)MUIA_Window_PublicScreen : (ULONG)TAG_IGNORE), (ULONG)MUI_PUB_NAME,

/* I659: MUI never closes a window by itself; the close gadget only sets MUIA_Window_CloseRequest. Every secondary window
 * closes itself on that request (the main window quits the application instead, see the notify in mui_open). */
static void mui_close_on_request(Object* win) {
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);
}

/* Reads the colours of the screen DrawInfo pens and decides the look once for all roles. Runs at window open and again for a live
 * THEME (I676c): the same decision from the tokens in force, the screen already open. */
static void mui_theme_decide(const uint32_t* token) {
    uint32_t pen_rgb[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int pen;
    struct Screen* scr;
    int i;
    scr = LockPubScreen(g_scr != NULL ? (CONST_STRPTR)MUI_PUB_NAME : NULL);
    if (scr != NULL) {
        struct DrawInfo* dri = GetScreenDrawInfo(scr);
        if (dri != NULL) {
            for (i = 0; i < 8; i++) {
                ULONG rgb[3] = {0, 0, 0};
                GetRGB32(scr->ViewPort.ColorMap, dri->dri_Pens[i], 1, rgb);
                pen_rgb[i] = ((rgb[0] >> 24) << 16) | ((rgb[1] >> 24) << 8) | (rgb[2] >> 24);
            }
            FreeScreenDrawInfo(scr, dri);
        }
        UnlockPubScreen(g_scr != NULL ? (CONST_STRPTR)MUI_PUB_NAME : NULL, scr);
    }
    /* I660: one decision for the whole look (mui_theme_pick_pen over DrawInfo pens 0..7). MUI 3.8 draws object text with
     * screen pens only; ESC plus a pen digit in PreParse picks one. No readable pen: every role stays off and the user's
     * MUI look is untouched. Otherwise all roles are on together and all text uses that pen. The string gadget (FIELD)
     * cannot take pen escapes, so it keeps the user's MUI look. */
    pen = mui_theme_pick_pen(pen_rgb, 8, token);
    g_text_rgb = token[SEL_GUI_TOKEN_TEXT];
    mui_theme_tokens_set(token);
    g_tokens = mui_theme_tokens();
    for (i = 0; i < MUI_TH_COUNT; i++) {
        g_th[i].on = mui_theme_look_for((mui_theme_role)i, token, &g_th[i].look) && pen >= 0 && i != MUI_TH_FIELD;
    }
}

static void mui_theme_init(const sel_ui_open_args* a) {
    txt_theme th;
    int i;
    const char* theme_arg = NULL;
    /* I676: ENV:Selecta/theme (Amarchy writes it), then --theme <file> over it, before any pen is allocated. */
    for (i = 0; a != NULL && i + 1 < a->argc; i++) {
        if (strcmp(a->argv[i], "--theme") == 0) {
            theme_arg = a->argv[i + 1];
        }
    }
    snprintf(g_theme_arg, sizeof g_theme_arg, "%s", theme_arg != NULL ? theme_arg : "");
    mui_font_file_add("Selecta:Fonts/Inter-Regular.ttf"); /* I685e2: Selecta's own font path; "RAM:Inter-Regular.ttf" was only the I665 spike's */
    mui_theme_resolve(&th, MUI_THEME_ENV_PATH, theme_arg); /* I676b: a THEME request made earlier wins over --theme */
    mui_screen_open(th.token);
    mui_trace("ui: screen, font, DrawInfo and pens done (mui_screen_open)");
    mui_theme_decide(th.token);
}

/* I676c: the live THEME. Same palette slots, new colours: the held pens of the own screen are re-loaded in place (SetRGB32), so
 * everything MUI and the stock objects drew with them follows; the AA classes then re-obtain their pens from the stored tokens
 * and redraw (aa_retheme_all). Pen numbers, the window tree, the lists and their rows are untouched: nothing is closed or opened. */
static void mui_live_palette(void* ctx, const uint32_t* tokens) {
    int i;
    (void)ctx;
    if (g_scr != NULL && mui_pens_plan_for(tokens, &g_plan)) {
        for (i = 0; i < MUI_PEN_TOTAL; i++) {
            if (g_pen_held[i] >= 0) {
                SetRGB32(&g_scr->ViewPort, (ULONG)g_pen_held[i], g_plan.rgb32[i][0], g_plan.rgb32[i][1], g_plan.rgb32[i][2]);
            }
        }
    }
    mui_theme_decide(tokens);
}

static void mui_live_classes(void* ctx) {
    (void)ctx;
    aa_retheme_all();
}

static int mui_theme_live_now(void) {
    mui_theme_live_ops ops;
    ops.palette = mui_live_palette;
    ops.classes = mui_live_classes;
    ops.ctx = NULL;
    /* I625a6a: a retheme re-probes the font files (the user may have installed one); the miss memory is otherwise kept per list. */
    mui_fontprobe_reset();
    return mui_theme_apply_live(&ops, MUI_THEME_ENV_PATH, g_theme_arg[0] != '\0' ? g_theme_arg : NULL);
}

/* I1139 (map/mui_text.h): uimodel text is UTF-8, MUI draws Latin-1. Selecta converts on its side of every handoff; the UP- classes
 * (plugins/shared/mcc) take Latin-1 untouched. These are the set calls and the read-back of a string gadget. */
static void mui_text_set(Object* obj, const char* utf8) {
    char b[MUI_L1_TEXT];
    mui_aatext_set(obj, mui_l1(b, sizeof b, utf8));
}

static void mui_caption_set(Object* obj, const char* utf8) {
    char b[MUI_L1_TEXT];
    mui_aabtn_set_caption(obj, mui_l1(b, sizeof b, utf8));
}

/* The string gadget's contents (Latin-1 as typed) as UTF-8 in buf[cap]. */
static const char* mui_str_utf8(Object* str, char* buf, size_t cap) {
    ULONG v = 0;
    GetAttr(MUIA_String_Contents, str, &v);
    mui_l1_to_utf8(buf, cap, v != 0 ? (const char*)v : "");
    return buf;
}

/* I667b (design 2026-10-28-mui-custom-classes.md section 3.1): the one button constructor. Every Selecta button (transport,
 * extras, doors, toolbar, crumbs) is a Selecta_AAButton (mui_aabtn_class.c): flat rounded face in the role and state token
 * colour, baked Lucide icon, antialiased caption. alt_key names a second icon shown after mui_aabtn_set_alt (play/pause).
 * icon_only buttons (dense rows) draw no caption so their row fits the smallest screen; the label stays the ShortHelp.
 * weight 100 shares the row, 0 sizes the button to its contents. Sizes come from mui_aa_metrics. */
static Object* mui_make_button_ex(const char* label, const char* widest, const char* icon_key, const char* alt_key, LONG id, LONG weight, int icon_only, int role, int align, int fixed_w, int fixed_h) {
    mui_aabtn_spec sp;
    sp.caption = label;
    sp.widest = widest;
    sp.help = NULL;
    sp.icon = mui_icon_id(icon_key);
    sp.alt_icon = alt_key != NULL ? mui_icon_id(alt_key) : -1;
    sp.icon_only = icon_only;
    sp.role = role;
    sp.id = id;
    sp.weight = weight;
    sp.selected = 0;
    sp.align = align;
    sp.fixed_w = fixed_w;
    sp.fixed_h = fixed_h;
    return mui_aabtn_new(&sp);
}

/* I973b: an icon-only button of an exact size (the player bar's squares: band.btn_w x band.btn_px), weight 0. */
static Object* mui_make_button_fixed(const char* label, const char* icon_key, const char* alt_key, LONG id, int role, int w, int h) {
    return mui_make_button_ex(label, label, icon_key, alt_key, id, 0, 1, role, MUI_AA_ALIGN_CENTRE, w, h);
}

static Object* mui_make_button_i(const char* label, const char* widest, const char* icon_key, const char* alt_key, LONG id, LONG weight, int icon_only, int role) {
    return mui_make_button_ex(label, widest, icon_key, alt_key, id, weight, icon_only, role, MUI_AA_ALIGN_CENTRE, 0, 0);
}

static Object* mui_make_button_w(const char* label, const char* icon_key, LONG id, LONG weight) {
    return mui_make_button_i(label, NULL, icon_key, NULL, id, weight, 0, MUI_AA_ROLE_GHOST); /* fit: breadcrumb captions change */
}

static Object* mui_make_button_icon(const char* label, const char* icon_key, LONG id) {
    return mui_make_button_i(label, label, icon_key, NULL, id, 100, 1, MUI_AA_ROLE_NORMAL);
}

/* I667b4: a ghost button with icon and caption at the left (sidebar doors). */
static Object* mui_make_button_left(const char* label, const char* icon_key, LONG id) {
    return mui_make_button_ex(label, label, icon_key, NULL, id, 100, 0, MUI_AA_ROLE_GHOST, MUI_AA_ALIGN_LEFT, 0, 0);
}

/* I667b-2 (design 3.1, AACheck): the four prefs checkboxes are Selecta_AAButton objects with role toggle: a rounded box, ACCENT
 * with a check icon when on. MUIA_Selected is the value (mui_p4_pref reads it) and every change returns id. */
static Object* mui_make_check(const char* label, int on, LONG id) {
    mui_aabtn_spec sp;
    sp.caption = label;
    sp.widest = label;
    sp.help = NULL;
    sp.icon = mui_icon_id("check");
    sp.alt_icon = -1;
    sp.icon_only = 1;
    sp.role = MUI_AA_ROLE_TOGGLE;
    sp.id = id;
    sp.weight = 0;
    sp.selected = on;
    sp.align = MUI_AA_ALIGN_CENTRE;
    sp.fixed_w = sp.fixed_h = 0;
    return mui_aabtn_new(&sp);
}

/* I667d (design 3.7): the one text constructor. Every Selecta text line is a Selecta_AAText (mui_aatext_class.c): antialiased in the
 * tone colour (normal TEXT, muted TEXT_MUTED, title TEXT at font + 4 px), aligned and cut with "..." at the width. Labels carry no
 * frame and no field box (I740, like the Mac footer: muted text on the window); the tone of a label comes from mui_pens_label_tone. */
static Object* mui_aatext_sized(const char* contents, const char* widest, int tone, int align, int hidden) {
    mui_aatext_spec sp;
    sp.contents = contents;
    sp.widest = widest;
    sp.fixed = widest != NULL;
    sp.tone = tone;
    sp.align = align;
    sp.ellipsis = 1;
    sp.frame = (ULONG)MUIV_Frame_None;
    sp.background = 0UL;
    sp.hidden = hidden;
    return mui_aatext_new(&sp);
}

static Object* mui_aatext(const char* contents, int tone, int align, int hidden) {
    return mui_aatext_sized(contents, NULL, tone, align, hidden);
}

/* I730: the one constructor of every dialog window (Settings, Help, the Commands palette). The attributes come from
 * mui_dialog_attrs: on the own screen the window is borderless, untitled, centred and fixed, and the root gets our flat header
 * row (title text + close button; the button takes Esc); on Workbench it keeps the normal borders and title. No per-dialog copy. */
static Object* mui_dialog_window(const char* title, ULONG id, Object* root) {
    mui_window_attrs_t da;
    Object *win, *content = root;
    mui_dialog_attrs(g_scr != NULL, &da);
    if (da.header != 0u) {
        mui_aabtn_spec sp;
        Object *hdr, *close, *col;
        sp.caption = "Close";
        sp.widest = "Close";
        sp.help = "Close (Esc)";
        sp.icon = mui_icon_id("close");
        sp.alt_icon = -1;
        sp.icon_only = 1;
        sp.role = MUI_AA_ROLE_GHOST;
        sp.id = CTL_DLG_CLOSE;
        sp.selected = 0;
        sp.weight = 0;
        sp.align = MUI_AA_ALIGN_CENTRE;
        sp.fixed_w = sp.fixed_h = 0;
        close = mui_aabtn_new_key(&sp, 27UL);
        hdr = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        col = MUI_NewObject((char*)MUIC_Group, TH_FONT TAG_DONE);
        if (close == NULL || hdr == NULL || col == NULL) {
            return NULL;
        }
        DoMethod(hdr, OM_ADDMEMBER, mui_aatext(title, MUI_AA_TONE_TITLE, MUI_AA_ALIGN_LEFT, 0));
        DoMethod(hdr, OM_ADDMEMBER, close);
        DoMethod(col, OM_ADDMEMBER, hdr);
        DoMethod(col, OM_ADDMEMBER, root);
        content = col;
        win = MUI_NewObject((char*)MUIC_Window, TH_SCR MUIA_Window_Title, (ULONG)NULL, MUIA_Window_ID, id, MUIA_Window_Borderless, (ULONG)da.borderless,
                            MUIA_Window_DragBar, (ULONG)da.dragbar, MUIA_Window_DepthGadget, (ULONG)da.depthgadget, MUIA_Window_SizeGadget, (ULONG)da.sizegadget,
                            MUIA_Window_CloseGadget, (ULONG)da.closegadget, MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
                            MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered, MUIA_Window_RootObject, (ULONG)content, TAG_DONE);
        if (win != NULL) {
            DoMethod(close, MUIM_Notify, MUIA_Pressed, FALSE, win, 3, MUIM_Set, MUIA_Window_Open, FALSE);
        }
        return win;
    }
    return MUI_NewObject((char*)MUIC_Window, TH_SCR MUIA_Window_Title, da.titled ? (ULONG)mui_l1_keep(title) : (ULONG)NULL, MUIA_Window_ID, id,
                         MUIA_Window_Borderless, (ULONG)da.borderless, MUIA_Window_DragBar, (ULONG)da.dragbar, MUIA_Window_DepthGadget, (ULONG)da.depthgadget,
                         MUIA_Window_SizeGadget, (ULONG)da.sizegadget, MUIA_Window_CloseGadget, (ULONG)da.closegadget, MUIA_Window_RootObject, (ULONG)content, TAG_DONE);
}

static Object* mui_make_text_al(mui_state* s, uint32_t attr, const char* initial, mui_label label, int align) {
    snprintf(s->txt_buf[attr], sizeof s->txt_buf[attr], "%s", initial);
    s->txt[attr] = mui_aatext_sized(s->txt_buf[attr], mui_attr_fixwidth(attr), mui_pens_label_tone(label), align, 0);
    return s->txt[attr];
}

static Object* mui_make_text(mui_state* s, uint32_t attr, const char* initial, mui_label label) {
    return mui_make_text_al(s, attr, initial, label, MUI_AA_ALIGN_LEFT);
}


/* I667f (design 3.2): the one slider constructor. Every Selecta slider (volume, seek, settings) is a Selecta_AASlider (mui_aaslider_class.c). */
static Object* mui_make_slider(long lo, long hi, long value, LONG id, long fix_w) {
    mui_aaslider_spec sp;
    sp.fix_w = fix_w;
    sp.lo = lo;
    sp.hi = hi;
    sp.value = value;
    sp.id = id;
    return mui_aaslider_new(&sp);
}

/* I798 (O3): give `obj` the ShortHelp from the one shared tip function (mui_tip) in the persistent buffer of `slot`. */
static void mui_set_tip(mui_state* s, uint32_t slot, Object* obj, uint32_t kind, uint32_t index, const sel_uim_frame* f, int flag) {
    if (s == NULL || obj == NULL || slot >= MUI_TS_COUNT) {
        return;
    }
    if (mui_tip(kind, index, f, flag, s->tip[slot], sizeof s->tip[slot]) > 0u) {
        char l1[sizeof s->tip[slot]];
        if (mui_l1(l1, sizeof l1, s->tip[slot]) == l1) { /* I1139: the tip buffer is rebuilt on every call, so convert it in place */
            memcpy(s->tip[slot], l1, sizeof l1);
        }
        SetAttrs(obj, MUIA_ShortHelp, (ULONG)s->tip[slot], TAG_DONE);
    }
}

/* I667e (design 3.8): the one panel constructor. Every Selecta panel group is a Selecta_AAPanel (mui_aapanel_class.c): a rounded face in
 * the role token colour (panel, surface, raised) over the window background. fixed_px > 0 is the square art frame. */
/* I671/I949b/I949c: the size of the screen the window opens on (the own SELECTA screen, else the default public screen) and whether
 * it is a user-chosen interlaced mode. 1280x720 when no screen can be locked. */
static void mui_screen_size(uint32_t* w, uint32_t* h, int* laced) {
    struct Screen* scr = g_scr != NULL ? g_scr : LockPubScreen(NULL);
    *w = 1280u;
    *h = 720u;
    *laced = 0;
    if (scr != NULL) {
        *w = (uint32_t)scr->Width;
        *h = (uint32_t)scr->Height;
        *laced = !g_screen_rtg && ((uint32_t)GetVPModeID(&scr->ViewPort) & MUI_MODE_LACE) != 0u;
        if (scr != g_scr) {
            UnlockPubScreen(NULL, scr);
        }
    }
}

/* I949c: the screen is the compact class (map/mui_layout.c mui_layout_compact): its sidebar is the door rail. */
static int mui_screen_compact(void) {
    uint32_t w, h;
    int laced;
    mui_screen_size(&w, &h, &laced);
    return mui_layout_compact(w, mui_layout_rows(h, laced), g_screen_rtg ? SEL_UIM_TIER_DEFAULT : SEL_UIM_TIER_BITMAP) != 0u;
}

/* fixed_h 0: the fixed frame is square; I949d: the compact player's art frame has its own height (tall pixels). */
static Object* mui_aapanel_wh(int role, int fixed_px, int fixed_h) {
    mui_aapanel_spec sp;
    sp.tokens = g_tokens;
    sp.role = role;
    sp.fixed_px = fixed_px;
    sp.fixed_h = fixed_h;
    return mui_aapanel_new(&sp);
}

static Object* mui_aapanel(int role, int fixed_px) {
    return mui_aapanel_wh(role, fixed_px, 0);
}

/* I793 (drift doc S3 to S9): the sidebar slot content. Everything shown comes from mui_sidebar_build (map/mui_sidebar.c): the doors
 * (Browse Folders left out, the header opens that page), the Your Library header with Create Playlist and Expand, the kind chips,
 * the item rows, the empty state and the Create your first playlist card. CTL_SIDE_BASE + MUI_SIDE_* routes the presses. */
static void mui_side_add(Object* g, Object* o) {
    if (o != NULL) {
        DoMethod(g, OM_ADDMEMBER, o);
    }
}

/* One left aligned sidebar line (a text of the sidebar model) and one left aligned button; each is one construction site. */
static void mui_side_text(Object* g, const char* text, int tone) {
    mui_side_add(g, mui_aatext(text, tone, MUI_AA_ALIGN_LEFT, 0));
}

/* I1166b: one table line of the expanded sidebar, four cells; the title cell is twice as wide as the others. */
static void mui_side_cells(Object* g, const char* a, const char* b, const char* c, const char* d, int tone) {
    Object* row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    const char* txt[4];
    uint32_t k;
    txt[0] = a; txt[1] = b; txt[2] = c; txt[3] = d;
    if (row == NULL) {
        return;
    }
    for (k = 0; k < 4u; k++) {
        Object* t = mui_aatext(txt[k], tone, MUI_AA_ALIGN_LEFT, 0);
        if (t != NULL) {
            SetAttrs(t, MUIA_HorizWeight, k == 0u ? 200 : 100, TAG_DONE);
            DoMethod(row, OM_ADDMEMBER, t);
        }
    }
    DoMethod(g, OM_ADDMEMBER, row);
}

static void mui_side_button(Object* g, const char* label, LONG id) {
    mui_side_add(g, mui_make_button_left(label, NULL, id));
}
static void mui_lib_press(mui_state* s, int dir, int chip);
static Object* mui_sidebar_dyn(const mui_sidebar_model* m);

/* I669: the dynamic part of the sidebar, S5 to S9, built from the model alone. I1092c: the part is a scrolled virtual group: a plain
 * group asks for the height of every library row, so with a few playlists the sidebar alone pushed the full layout's minimum to
 * 638 rows and the 800x600 screen refused it (the open ladder fell to the mini layout). A scrolled group asks for scroll bars only. */
static Object* mui_sidebar_dyn(const mui_sidebar_model* m) {
    Object* d = MUI_NewObject((char*)MUIC_Virtgroup, TAG_DONE);
    Object* sg;
    uint32_t i;
    if (d == NULL) {
        return NULL;
    }
    if (m->chip_single) { /* S5, chips do not fit: one label naming the filter in force between Previous and Next, the label takes no click (I669) */
        Object* row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        if (row != NULL) {
            DoMethod(row, OM_ADDMEMBER, mui_make_button_icon(m->chip_left_name, m->chip_left_clear ? "close" : "prev_album", CTL_LIB_PREV));
            mui_side_add(row, mui_aatext(m->chip_label, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_CENTRE, 0));
            DoMethod(row, OM_ADDMEMBER, mui_make_button_icon("Next Filter", "next_album", CTL_LIB_NEXT));
            DoMethod(d, OM_ADDMEMBER, row);
        }
    } else { /* S5, chips fit: one row, a click toggles the filter */
        Object* row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        for (i = 0; row != NULL && i < SEL_UIM_SIDE_CHIPS; i++) {
            if (m->chip_shown[i]) {
                mui_side_button(row, m->chip[i], CTL_SIDE_BASE + MUI_SIDE_CHIP + (LONG)i);
            }
        }
        mui_side_add(d, row);
    }
    if (m->wide) {
        mui_side_cells(d, m->head[0], m->head[1], m->head[2], m->head[3], MUI_AA_TONE_MUTED);
    }
    for (i = 0; i < m->row_n; i++) { /* S6: mark, title, subtitle; the model rows are the filtered rows, the only row list */
        char line[200];
        snprintf(line, sizeof line, "%s  %s  %s", m->row[i].mark, m->row[i].title, m->row[i].subtitle);
        if (m->wide) { /* I1166b: Expand Library, the table: Title | Type | Date Added | Played (the Mac side_table rows) */
            mui_side_cells(d, line, m->row[i].type, m->row[i].added, m->row[i].played, MUI_AA_TONE_NORMAL);
        } else {
            mui_side_text(d, line, MUI_AA_TONE_NORMAL);
        }
    }
    if (m->empty[0] != '\0') { /* S8 */
        mui_side_text(d, m->empty, MUI_AA_TONE_MUTED);
    }
    if (m->card) { /* S9 */
        mui_side_text(d, m->card_title, MUI_AA_TONE_TITLE);
        mui_side_text(d, m->card_body, MUI_AA_TONE_MUTED);
        mui_side_button(d, m->card_button, CTL_SIDE_BASE + MUI_SIDE_CARD);
    }
    DoMethod(d, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    sg = MUI_NewObject((char*)MUIC_Scrollgroup, MUIA_Scrollgroup_FreeHoriz, FALSE, MUIA_Scrollgroup_Contents, d, TAG_DONE);
    if (sg == NULL) {
        MUI_DisposeObject(d);
    }
    return sg;
}

/* The model of the sidebar for the filter in force: the rows are s->side_items, passed by sel_uim_lfilter_pass (mui_sidebar_fill). */
static void mui_sidebar_model_for(mui_state* s, int top_bar, mui_sidebar_model* m) {
    mui_sidebar_in in;
    memset(&in, 0, sizeof in);
    in.top_bar = top_bar;
    in.bitmap = 0;
    in.rail = s != NULL && s->side_rail; /* I949c */
    in.chip = s != NULL ? s->lib_filter : 0u;
    in.wide = s != NULL && s->lay.side_wide; /* I1166b: Expand Library */
    in.now = (int64_t)time(NULL);
    in.chips_single = mui_sidebar_chips_single(!g_screen_rtg, s != NULL && s->lay.weight_w[MUI_SLOT_SIDEBAR] > 0u ? s->lay.weight_w[MUI_SLOT_SIDEBAR] : 0u, g_pixel_aspect_q8);
    in.filter = "";
    in.item_n = s != NULL ? s->side_item_n : 0u;
    in.items = s != NULL ? s->side_items : NULL;
    mui_sidebar_build(&in, m);
    if (s != NULL) {
        s->side_single = m->chip_single;
    }
}

static Object* mui_sidebar_group(mui_state* s, int top_bar) {
    Object* g = mui_aapanel(MUI_AA_PANEL_PANEL, 0);
    mui_sidebar_model m;
    Object* hd;
    uint32_t i, k;
    if (g == NULL) {
        return NULL;
    }
    if (s != NULL) {
        s->side_top_bar = top_bar;
        s->side_rail = mui_screen_compact(); /* I949c: the compact class builds the door rail, not the Your Library block */
        s->side_item_n = s->side_rail ? 0u : sel_uim_lfilter_pass(s->engine, s->lib_filter, s->side_items, MUI_SIDEBAR_ROWS, NULL); /* the shared filter pass */
    }
    mui_sidebar_model_for(s, top_bar, &m);
    if (m.rail) { /* I949c: the rail packs the doors one pixel apart (the Mac layout is the structure reference, not the spacing) */
        SetAttrs(g, MUIA_Group_Spacing, MUI_RAIL_SPACING, TAG_DONE);
    }
    hd = m.rail ? NULL : MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE); /* S1: the brand, an accent logo tile then the name (draw_sidebar) */
    if (hd != NULL) {
        if (m.brand_icon[0] != '\0') {
            mui_side_add(hd, mui_make_button_i(m.brand, m.brand, m.brand_icon, NULL, 0, 0, 1, MUI_AA_ROLE_ACCENT)); /* id 0: not a control */
        }
        mui_side_text(hd, m.brand, MUI_AA_TONE_TITLE);
        DoMethod(g, OM_ADDMEMBER, hd);
    }
    for (k = 0; k < m.door_n; k++) { /* S3 */
        i = m.door[k];
        Object* b = mui_make_button_left(mui_door_label(i), sel_uim_door_at(i)->page, CTL_DOOR_BASE + (LONG)i);
        if (s != NULL && i < MUI_SIDEBAR_DOORS) {
            s->door_btn[i] = b; /* S2: lit by mui_route_sync */
            mui_set_tip(s, MUI_TS_DOOR + i, b, MUI_TIP_DOOR, i, NULL, 0);
            SetAttrs(b, UPA_Shared_Side, MUI_AA_TIP_RIGHT, TAG_DONE); /* I731: the sidebar door tip opens beside the door, not over the next one */
        }
        mui_side_add(g, b);
    }
    if (m.rail) {
        /* I963: the doors have a bounded maximum height, and a horizontal MUI group takes the smallest maximum height of its
         * children: without a stretchable child the rail capped the whole body row (and the window) at the doors' height,
         * so the compact window stopped at 184 of 256 rows. The full sidebar ends in the same filler (mui_sidebar_dyn). */
        DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    }
    hd = m.rail ? NULL : MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE); /* S4: title, Create Playlist, Expand */
    if (hd != NULL) {
        Object* t = mui_make_button_ex(m.title, m.title, "library", NULL, CTL_SIDE_BASE + MUI_SIDE_TITLE, 100, 0, MUI_AA_ROLE_GHOST, MUI_AA_ALIGN_LEFT, 0, 0);
        Object* c = mui_make_button_ex(m.create, m.create, "New Playlist", NULL, CTL_SIDE_BASE + MUI_SIDE_CREATE, 0, 1, MUI_AA_ROLE_GHOST, MUI_AA_ALIGN_CENTRE, 0, 0);
        Object* x = mui_make_button_ex(m.expand, m.expand, "nav_forward", "nav_back", CTL_SIDE_BASE + MUI_SIDE_EXPAND, 0, 1, MUI_AA_ROLE_GHOST, MUI_AA_ALIGN_CENTRE, 0, 0);
        if (t != NULL) { DoMethod(hd, OM_ADDMEMBER, t); }
        if (c != NULL) { DoMethod(hd, OM_ADDMEMBER, c); }
        if (x != NULL) { DoMethod(hd, OM_ADDMEMBER, x); }
        if (s != NULL) { s->side_expand_btn = x; if (x != NULL) { mui_aabtn_set_alt(x, s->lay.side_wide != 0u); } }
        DoMethod(g, OM_ADDMEMBER, hd);
    }
    if (s != NULL && !m.rail) {
        s->side_dyn = mui_sidebar_dyn(&m);
        mui_side_add(g, s->side_dyn);
    }
    return g;
}

/* MUI-P8-1: the album and speed buttons, one row under the transport row. Labels and help come from the action table
 * (mui_extra_*); a press goes through mui_action_fire in mui_control_fire. */
static int mui_p8_send(void* ctx, const char* line) {
    return mui_engine_line((mui_state*)ctx, line);
}

/* I973c (design 2026-11-02-I973-mui-player-bar-design.md sections 3, 4, 6a): the player bar is ONE row, for the bitmap screen and RTG alike.
 * Left to right: art and titles (only when the band says so), the subsong group (hidden until the song has subsongs), five square transport
 * icons (Random, Previous, Play, Next, Repeat; no Stop), the clock, the seek slider (the only child that stretches), the length, the speaker
 * and the volume slider. Every size comes from mui_player_band (map layer); the group height is fixed so every child sits on the centre line. */
static Object* mui_player_group(mui_state* s) {
    sel_uim_frame zero;
    Object *g, *bar, *tr, *art, *titles, *left, *sub, *sub_prev, *sub_next, *speaker;
    mui_player_band_t band;
    uint32_t k;
    memset(&zero, 0, sizeof zero);
    {
        uint32_t sw, sh;
        int laced;
        mui_screen_size(&sw, &sh, &laced);
        mui_player_band(&band, mui_screen_compact() ? 1u : 0u, g_pixel_aspect_q8, sw, (uint32_t)mui_aa_font_px((int)mui_layout_rows(sh, laced), (int)sw)); /* I949d, I973a */
    }
    g = mui_aapanel(MUI_AA_PANEL_SURFACE, 0);
    bar = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, (ULONG)band.gap, TAG_DONE);
    tr = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, (ULONG)band.btn_gap, MUIA_Weight, 0, TAG_DONE);
    sub = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, (ULONG)band.btn_gap, MUIA_Weight, 0, MUIA_ShowMe, FALSE, TAG_DONE);
    if (g == NULL || bar == NULL || tr == NULL || sub == NULL) {
        return NULL;
    }
    SetAttrs(g, MUIA_FixHeight, (ULONG)band.height, TAG_DONE);
    if (band.show_left) {
        art = mui_aapanel_wh(MUI_AA_PANEL_RAISED, (int)band.art_w, (int)band.art_h);
        titles = MUI_NewObject((char*)MUIC_Group, MUIA_Weight, 0, MUIA_FixWidth, (ULONG)MUI_PLAYER_TITLES_W, TAG_DONE);
        left = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, (ULONG)band.gap, MUIA_Weight, 0, TAG_DONE);
        if (art == NULL || titles == NULL || left == NULL) {
            return NULL;
        }
        DoMethod(art, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE)); /* a group needs one child; the cover itself is the Now Playing art */
        DoMethod(titles, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_TITLE, "No song", MUI_LBL_TITLE));
        DoMethod(titles, OM_ADDMEMBER, mui_make_text(s, MUI_ATTR_ARTIST, "", MUI_LBL_ARTIST));
        DoMethod(left, OM_ADDMEMBER, art);
        DoMethod(left, OM_ADDMEMBER, titles);
        DoMethod(bar, OM_ADDMEMBER, left);
    }
    sub_prev = mui_make_button_fixed("Previous Subsong", "minus", NULL, CTL_SUB_PREV, MUI_AA_ROLE_NORMAL, (int)band.btn_w, (int)band.btn_px);
    sub_next = mui_make_button_fixed("Next Subsong", "plus", NULL, CTL_SUB_NEXT, MUI_AA_ROLE_NORMAL, (int)band.btn_w, (int)band.btn_px);
    s->sub_buf[0] = '\0';
    s->sub_txt = mui_aatext_sized(s->sub_buf, "00/00", mui_pens_label_tone(MUI_LBL_CLOCK), MUI_AA_ALIGN_CENTRE, 0);
    if (sub_prev == NULL || sub_next == NULL || s->sub_txt == NULL) {
        return NULL;
    }
    SetAttrs(sub_prev, MUIA_ShortHelp, (ULONG) "Previous subsong", TAG_DONE);
    SetAttrs(sub_next, MUIA_ShortHelp, (ULONG) "Next subsong", TAG_DONE);
    DoMethod(sub, OM_ADDMEMBER, sub_prev);
    DoMethod(sub, OM_ADDMEMBER, s->sub_txt);
    DoMethod(sub, OM_ADDMEMBER, sub_next);
    s->sub_grp = sub;
    s->sub_shown = 0u;
    DoMethod(bar, OM_ADDMEMBER, sub);
    for (k = 0; k < band.n_transport; k++) {
        uint32_t i = mui_player_transport_at(k);
        s->btn_label[i] = mui_transport_label(i, &zero);
        s->btn[i] = mui_make_button_fixed(s->btn_label[i], mui_transport_action(i, &zero), i == MUI_BTN_PLAY ? "pause" : NULL, CTL_BTN_BASE + (LONG)i,
                                          i == MUI_BTN_PLAY ? MUI_AA_ROLE_ACCENT : MUI_AA_ROLE_NORMAL, (int)band.btn_w, (int)band.btn_px);
        if (s->btn[i] != NULL) {
            mui_set_tip(s, MUI_TS_BTN + i, s->btn[i], MUI_TIP_TRANSPORT, i, &zero, 0);
            DoMethod(tr, OM_ADDMEMBER, s->btn[i]);
        }
    }
    DoMethod(bar, OM_ADDMEMBER, tr);
    s->seek = mui_make_slider(0, MUI_SEEK_MAX, 0, CTL_SEEK, 0);
    s->vol = mui_make_slider(0, MUI_VOLUME_MAX, 0, CTL_VOLUME, MUI_VOLUME_FIX_W);
    speaker = mui_make_button_fixed("Volume", "volume-2", NULL, 0, MUI_AA_ROLE_GHOST, (int)band.btn_w, (int)band.btn_px); /* an indicator: no mute action exists */
    if (s->seek == NULL || s->vol == NULL || speaker == NULL) {
        return NULL;
    }
    mui_set_tip(s, MUI_TS_SEEK, s->seek, MUI_TIP_SEEK, 0, NULL, 0);
    mui_set_tip(s, MUI_TS_VOL, s->vol, MUI_TIP_VOLUME, 0, NULL, 0);
    DoMethod(bar, OM_ADDMEMBER, mui_make_text_al(s, MUI_ATTR_CLOCK, "0:00 ", MUI_LBL_CLOCK, MUI_AA_ALIGN_RIGHT));
    DoMethod(bar, OM_ADDMEMBER, s->seek);
    DoMethod(bar, OM_ADDMEMBER, mui_make_text_al(s, MUI_ATTR_LENGTH, "0:00 ", MUI_LBL_LENGTH, MUI_AA_ALIGN_LEFT));
    DoMethod(bar, OM_ADDMEMBER, speaker);
    DoMethod(bar, OM_ADDMEMBER, s->vol); /* I717: volume at the right end of the bar, like the Mac player bar */
    DoMethod(g, OM_ADDMEMBER, bar);
    return g;
}

/* I973c: show or hide the bar's subsong group and keep its "n/m" label on the frame. The text is touched only when it changes. */
static void mui_subsong_sync(mui_state* s, const sel_uim_frame* f) {
    char label[sizeof s->sub_buf];
    uint32_t show = mui_subsong_label(f->subsong, f->subsong_count, label, sizeof label);
    if (s->sub_grp == NULL) {
        return;
    }
    if (show && strcmp(label, s->sub_buf) != 0) {
        snprintf(s->sub_buf, sizeof s->sub_buf, "%s", label);
        mui_text_set(s->sub_txt, s->sub_buf);
    }
    if (show != s->sub_shown) {
        s->sub_shown = show;
        set(s->sub_grp, MUIA_ShowMe, show ? TRUE : FALSE);
    }
}

/* MUI-P3-4: the List's display hook. The entry is the row's cell array; a NULL entry asks for the column titles
 * (title row on). The columns shown are mui_columns_for(breakpoint): the rest are hidden by width. */
static struct Hook g_list_hook;
static uint32_t g_list_hook_calls;
/* The text of column `col` of a row's cells. */
static const char* mui_cell_col(const mui_cells* cells, uint8_t col) {
    return col == MUI_COL_TITLE ? cells->title : col == MUI_COL_ARTIST ? cells->artist : col == MUI_COL_ALBUM ? cells->extra.album : col == MUI_COL_LENGTH ? cells->extra.length : cells->extra.format;
}

static ULONG mui_list_display(struct Hook* hook, char** array, mui_cells* cells) {
    static const char* const titles[MUI_COL_COUNT] = {"Title", "Artist", "Album", "Length", "Format"};
    uint8_t cols[MUI_COL_COUNT];
    uint32_t n = mui_columns_for(g_state.col_bp, cols), k;
    (void)hook;
    g_list_hook_calls++;
    for (k = 0; k < n; k++) {
        const char* t = cells == NULL ? titles[cols[k]] : mui_cell_col(cells, cols[k]);
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

/* I733: the search field hint, from the shared model; the top bar form on the RTG tier, else the facet form. */
static const char* mui_search_hint(void) {
    static char hint[48];
    sel_uim_search_hint(g_screen_rtg, sel_uim_search_facets, SEL_UIM_SEARCH_FACETS, hint, sizeof hint);
    return hint;
}

/* MUI-P3-1: the 3.8 Listview with its plain List (the back end mui_list_backend_pick gives without NList). */
static Object* mui_make_string(LONG id, int max_w, const char* placeholder) {
    /* I667h (design 3.4): every string gadget is a Selecta_AAField (mui_aafield_class.c). */
    mui_aafield_spec sp;
    sp.tokens = g_tokens;
    sp.maxlen = 159;
    sp.id = id;
    sp.max_w = max_w;
    sp.placeholder = placeholder;
    return mui_aafield_new(&sp);
}

/* I684a: the initial entry goes in at OM_NEW. Setting the active entry before the object is in the application tree fires the
 * MUIV_Notify_Application notification with no global info yet; MUI then reads the application through a NULL pointer and
 * the Amiga crashed at start whenever the saved visualizer (or a settings cycle) was not the first entry. */
static Object* mui_make_cycle(const char* const* entries, LONG active, LONG id) {
    /* I667g (design 3.3): every cycle is a Selecta_AACycle (mui_aacycle_class.c). */
    mui_aacycle_spec sp;
    sp.entries = entries;
    sp.active = active;
    sp.id = id;
    return mui_aacycle_new(&sp);
}

/* I615d (design 8.3): the window-wide top bar of the DEFAULT tier, centred like topbar_compute: Home (round icon button), the search field
 * and Browse joined to its right end by a 1 px divider. Built only on an RTG screen; on the bitmap tier the slot is an empty hidden group
 * (the search field then stays in the tool row). One search String and one control id (CTL_P4_BASE + P4_SEARCH) either way. */
static Object* mui_topbar_group(mui_state* s) {
    static char div_spec[40];
    Object* g = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    if (g == NULL || !g_screen_rtg) {
        return g;
    }
    s->str_search = mui_make_string(CTL_P4_BASE + P4_SEARCH, (int)SEL_UIM_TOPBAR_FIELD_MAX, mui_search_hint());
    mui_set_tip(s, MUI_TS_SEARCH, s->str_search, MUI_TIP_SEARCH, 0, NULL, 0);
    mui_theme_bg_spec(g_tokens[SEL_GUI_TOKEN_DIVIDER], div_spec, sizeof div_spec);
    DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    s->nav_back = mui_make_button_i("Back", "Back", "nav_back", NULL, CTL_NAV_BACK, 0, 1, MUI_AA_ROLE_NORMAL); /* I794 (T1): ahead of Home */
    s->nav_fwd = mui_make_button_i("Forward", "Forward", "nav_forward", NULL, CTL_NAV_FWD, 0, 1, MUI_AA_ROLE_NORMAL);
    if (s->nav_back != NULL && s->nav_fwd != NULL) {
        set(s->nav_back, MUIA_Disabled, TRUE);
        set(s->nav_fwd, MUIA_Disabled, TRUE);
        DoMethod(g, OM_ADDMEMBER, s->nav_back);
        DoMethod(g, OM_ADDMEMBER, s->nav_fwd);
    }
    DoMethod(g, OM_ADDMEMBER, mui_make_button_i("Home", "Home", "home", NULL, CTL_DOOR_BASE + 0, 0, 1, MUI_AA_ROLE_NORMAL));
    if (s->str_search != NULL) {
        DoMethod(g, OM_ADDMEMBER, s->str_search);
    }
    DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, MUIA_FixWidth, 1, MUIA_Background, (ULONG)div_spec, TAG_DONE));
    DoMethod(g, OM_ADDMEMBER, mui_make_button_i("Browse", "Browse", "library", NULL, CTL_TOPBAR_BROWSE, 0, 1, MUI_AA_ROLE_NORMAL));
    DoMethod(g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    s->pal_btn = mui_make_button_i("Commands", "Commands", "Help", NULL, CTL_PAL_BTN, 0, 1, MUI_AA_ROLE_NORMAL); /* I798 (O1) */
    s->np_btn = mui_make_button_i(sel_uim_tip_np_panel(0), sel_uim_tip_np_panel(0), "nav_forward", "nav_back", CTL_NP_TOGGLE, 0, 1, MUI_AA_ROLE_NORMAL); /* I798 (T2, R3): the right end */
    if (s->pal_btn != NULL) {
        mui_set_tip(s, MUI_TS_PAL, s->pal_btn, MUI_TIP_PALETTE, 0, NULL, 0);
        DoMethod(g, OM_ADDMEMBER, s->pal_btn);
    }
    if (s->np_btn != NULL) {
        mui_set_tip(s, MUI_TS_NP, s->np_btn, MUI_TIP_NP_PANEL, 0, NULL, (int)s->lay.detail_hidden);
        DoMethod(g, OM_ADDMEMBER, s->np_btn);
    }
    return g;
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
    if (!g_screen_rtg) { /* I615d: on the DEFAULT tier the search field lives in the top bar */
        s->str_search = mui_make_string(CTL_P4_BASE + P4_SEARCH, 0, mui_search_hint());
        mui_set_tip(s, MUI_TS_SEARCH, s->str_search, MUI_TIP_SEARCH, 0, NULL, 0);
    }
    s->str_cmd = mui_make_string(CTL_P4_BASE + P4_CMD, 0, SEL_UIM_CMD_HINT);
    mui_set_tip(s, MUI_TS_CMD, s->str_cmd, MUI_TIP_CMD, 0, NULL, 0);
    s->cyc_rating = mui_make_cycle(p4_rating_entries, 0, CTL_P4_BASE + P4_RATING);
    s->cyc_subsong = mui_make_cycle(p4_subsong_entries, 0, CTL_P4_BASE + P4_SUBSONG);
    snprintf(s->status_buf, sizeof s->status_buf, "Ready");
    s->status = mui_aatext(s->status_buf, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 0);
    if ((!g_screen_rtg && s->str_search == NULL) || s->str_cmd == NULL || s->cyc_rating == NULL || s->cyc_subsong == NULL || s->status == NULL) {
        return NULL;
    }
    if (!g_screen_rtg) {
        DoMethod(r1, OM_ADDMEMBER, s->str_search);
    }
    DoMethod(r1, OM_ADDMEMBER, s->str_cmd);
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if (g_tool_which[i] == P4_LIKE) {
            continue; /* I1125: Like lives on the Now Playing panel (Mac location), not in the tool row */
        }
        /* New Playlist is the accent button of the row (design 3.1); the others are plain icon buttons. */
        Object* b = g_tool_which[i] == P4_PNEW
                        ? mui_make_button_i(mui_tool_labels[i], mui_tool_labels[i], mui_tool_labels[i], NULL, CTL_P4_BASE + (LONG)g_tool_which[i], 100, 1, MUI_AA_ROLE_ACCENT)
                        : mui_make_button_icon(mui_tool_labels[i], mui_tool_labels[i], CTL_P4_BASE + (LONG)g_tool_which[i]);
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

/* I667i2: the main list's columns (weights and fixed-width samples) for a breakpoint. I1079d: a column no listed row has text for (the
 * Artist or Album of a podcast list) is absent, as in the desktop (design 2026-11-05-I1079 section 3.4): weight 0 and no sample, so the shared
 * splitter gives it width 0 and no gap. The Title column is never dropped, and an empty list keeps every column. */
static void mui_main_cols_fill(uint32_t bp, mui_aalist_cols* out, const mui_cells* cells, uint32_t cell_n) {
    static const char* const sample[MUI_COL_COUNT] = {NULL, NULL, NULL, "00:00:00", "Format"};
    uint8_t cols[MUI_COL_COUNT];
    uint32_t k, n;
    memset(out, 0, sizeof *out);
    n = mui_columns_for(bp, cols);
    out->n = (int)mui_columns_weights(bp, out->weight);
    for (k = 0; k < n && k < MUI_AALIST_COLS; k++) {
        uint32_t r, has = cell_n == 0u || cols[k] == MUI_COL_TITLE;
        for (r = 0; !has && r < cell_n; r++) {
            has = *mui_cell_col(&cells[r], cols[k]) != '\0';
        }
        out->sample[k] = has ? sample[cols[k]] : NULL;
        if (!has) {
            out->weight[k] = 0;
        }
    }
}

/* Refill the main list's columns from the rows now listed and hand them to the list (the only place the columns attribute is set). */
static void mui_main_cols_push(mui_state* s) {
    mui_main_cols_fill(s->col_bp, &s->main_cols, s->cell, s->cell_n);
    set(s->list, UPA_List_Columns, (ULONG)&s->main_cols);
}

/* I667i2: the AAList cell callback of the main list; the display hook stays the one source of the texts (row -1: the column titles). */
static const char* mui_main_cell(void* ctx, long row, int col) {
    mui_state* s = (mui_state*)ctx;
    char* arr[MUI_COL_COUNT];
    uint32_t k;
    if (col < 0 || col >= MUI_COL_COUNT || (row >= 0 && s->entry_ptr[row] == NULL)) {
        return "";
    }
    for (k = 0; k < MUI_COL_COUNT; k++) {
        arr[k] = (char*)"";
    }
    (void)mui_list_display(&g_list_hook, arr, row < 0 ? NULL : (mui_cells*)s->entry_ptr[row]);
    return arr[col];
}

/* I667i2: one row per string of a NULL-terminated pointer array (info, tracks, help). */
static const char* mui_ptr_row(void* ctx, long row) {
    return ((const char* const*)ctx)[row];
}

/* I667i2: the lyrics row callback: line row of np_lyric_buf. */
static const char* mui_lyric_row(void* ctx, long row) {
    mui_state* s = (mui_state*)ctx;
    mui_text_line(s->np_lyric_buf, (uint32_t)row, s->lyric_line, sizeof s->lyric_line);
    return s->lyric_line;
}

/* One AAList (list and flat AAScroller) over a string array; *list_out is the list object. */
static Object* mui_row_aalist(const char* (*row_fn)(void*, long), void* ctx, long count, int rows, Object** list_out) {
    mui_aalist_spec sp;
    memset(&sp, 0, sizeof sp);
    sp.count = count;
    sp.active = -1;
    sp.row = row_fn;
    sp.ctx = ctx;
    sp.max_rows = rows;
    return mui_aalist_new(&sp, list_out);
}

static Object* mui_string_aalist(void* ctx, long count, int rows, Object** list_out) {
    return mui_row_aalist(mui_ptr_row, ctx, count, rows, list_out);
}

/* I794 (P3): row `row` of the collection page (rows are "path\0plays\0" pairs): the song's catalogued title (I1306b), else the file name
 * of its path. */
static const char* mui_col_row(void* ctx, long row) {
    mui_state* s = (mui_state*)ctx;
    const sel_collection_page* pg = sel_engine_state(s->engine)->collection;
    const char* r;
    const char* base;
    long i;
    if (pg == NULL || pg->rows == NULL || row < 0 || (uint32_t)row >= pg->songs) {
        return "";
    }
    r = sel_uim_briefs_title(&s->col_brf, pg, (uint32_t)row);
    if (r != NULL) {
        return r;
    }
    for (r = pg->rows, i = 0; i < row && *r != '\0'; i++) {
        r += strlen(r) + 1u;
        r += strlen(r) + 1u;
    }
    base = strrchr(r, '/');
    if (base == NULL) {
        base = strrchr(r, ':');
    }
    return base != NULL ? base + 1 : r;
}

static int mui_route_send(void* ctx, const char* line) {
    mui_state* s = (mui_state*)ctx;
    if (line != NULL && strcmp(line, "SOURCES") == 0) {
        mui_srcs_asked(&s->srcs); /* I1175: the next SOURCES rows replace the Sources page rows */
    }
    return mui_engine_line(s, line);
}

/* I1172: a playlist row opens its collection page like the desktop sidebar row (SIDEBAR_ROW, key = the playlist id). */
static int mui_playlist_page(mui_state* s, const sel_view_row* row, const char* name) {
    char key[16];
    snprintf(key, sizeof key, "%u", (unsigned)row->id);
    return mui_route_click(&s->route, SEL_ROUTE_SIDEBAR_ROW, (uint32_t)SEL_LIB_ITEM_PLAYLIST, row->id, key, name, "", mui_route_send, s);
}

static Object* mui_col_page_group(mui_state* s) {
    mui_aalist_spec sp;
    Object* lg;
    Object* head = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    Object* g = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
    snprintf(s->col_title_buf, sizeof s->col_title_buf, " ");
    snprintf(s->col_meta_buf, sizeof s->col_meta_buf, " ");
    s->col_title = mui_aatext(s->col_title_buf, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 1);
    s->col_meta = mui_aatext(s->col_meta_buf, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 0);
    s->col_artist = mui_make_button_w("Artist", "Artist", CTL_COL_ARTIST, 0);
    memset(&sp, 0, sizeof sp);
    sp.active = -1;
    sp.row = mui_col_row;
    sp.ctx = s;
    sp.max_rows = 12;
    sp.double_pick = 1;
    lg = mui_aalist_new(&sp, &s->col_list);
    if (g == NULL || head == NULL || s->col_title == NULL || s->col_meta == NULL || s->col_artist == NULL || lg == NULL || s->col_list == NULL) {
        return NULL;
    }
    DoMethod(s->col_list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_COL_LIST);
    DoMethod(head, OM_ADDMEMBER, s->col_title);
    DoMethod(head, OM_ADDMEMBER, s->col_artist);
    DoMethod(g, OM_ADDMEMBER, head);
    DoMethod(g, OM_ADDMEMBER, s->col_meta);
    DoMethod(g, OM_ADDMEMBER, lg);
    return g;
}

/* I795 (P1): row `row` of a Home shelf list: the text of the engine view row the slot lists from (shelf_first). */
static const char* mui_shelf_row(void* ctx, long row) {
    mui_shelf_ctx* c = (mui_shelf_ctx*)ctx;
    const sel_view_state* v = sel_engine_state(c->s->engine)->view;
    uint32_t idx = c->s->shelf_first[c->slot] + (uint32_t)row;
    if (v == NULL || v->rows == NULL || row < 0 || idx >= v->row_n || v->rows[idx].text == NULL) {
        return "";
    }
    return v->rows[idx].text;
}

/* I795 (P1): the Home page group: the quick tile row, then up to MUI_HOME_SHELF_MAX shelves (header Text, Show all button, a short AAList).
 * Hidden until mui_route_sync shows it; the slots fill from the view (mui_home_split). */
static Object* mui_home_page_group(mui_state* s) {
    Object* g = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
    uint32_t k;
    s->quick_row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    if (g == NULL || s->quick_row == NULL) {
        return NULL;
    }
    for (k = 0; k < MUI_HOME_QUICK_MAX; k++) {
        s->quick_btn[k] = mui_make_button_w(" ", "tile", CTL_QUICK_BASE + (LONG)k, 100);
        if (s->quick_btn[k] == NULL) {
            return NULL;
        }
        set(s->quick_btn[k], MUIA_ShowMe, FALSE);
        DoMethod(s->quick_row, OM_ADDMEMBER, s->quick_btn[k]);
    }
    DoMethod(g, OM_ADDMEMBER, s->quick_row);
    for (k = 0; k < MUI_HOME_SHELF_MAX; k++) {
        mui_aalist_spec sp;
        Object* lg;
        Object* head = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        s->shelf_grp[k] = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
        snprintf(s->shelf_head_buf[k], sizeof s->shelf_head_buf[k], " ");
        s->shelf_head[k] = mui_aatext(s->shelf_head_buf[k], MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 0);
        s->shelf_all[k] = mui_make_button_w("Show all", "tile", CTL_SHELF_ALL_BASE + (LONG)k, 0);
        s->shelf_ctx[k].s = s;
        s->shelf_ctx[k].slot = k;
        memset(&sp, 0, sizeof sp);
        sp.active = -1;
        sp.row = mui_shelf_row;
        sp.ctx = &s->shelf_ctx[k];
        sp.max_rows = (int)MUI_HOME_SHELF_ROWS;
        sp.double_pick = 1;
        lg = mui_aalist_new(&sp, &s->shelf_list[k]);
        if (s->shelf_grp[k] == NULL || head == NULL || s->shelf_head[k] == NULL || s->shelf_all[k] == NULL || lg == NULL || s->shelf_list[k] == NULL) {
            return NULL;
        }
        DoMethod(s->shelf_list[k], MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)(CTL_SHELF_LIST_BASE + (LONG)k));
        DoMethod(head, OM_ADDMEMBER, s->shelf_head[k]);
        DoMethod(head, OM_ADDMEMBER, s->shelf_all[k]);
        DoMethod(s->shelf_grp[k], OM_ADDMEMBER, head);
        DoMethod(s->shelf_grp[k], OM_ADDMEMBER, lg);
        DoMethod(g, OM_ADDMEMBER, s->shelf_grp[k]);
    }
    return g;
}

/* I795 (P4): the Browse page: the tiles of mui_browse_tiles in a two column grid (FORMATS_SHOW, COMPOSERS_SHOW, SOURCES). */
static Object* mui_browse_page_group(mui_state* s) {
    Object* g = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Columns, 2, MUIA_ShowMe, FALSE, TAG_DONE);
    uint32_t k;
    if (g == NULL) {
        return NULL;
    }
    for (k = 0; k < MUI_TILE_MAX; k++) {
        s->tile_btn[k] = mui_make_button_w(" ", "tile", CTL_TILE_BASE + (LONG)k, 100);
        if (s->tile_btn[k] == NULL) {
            return NULL;
        }
        set(s->tile_btn[k], MUIA_ShowMe, FALSE);
        DoMethod(g, OM_ADDMEMBER, s->tile_btn[k]);
    }
    return g;
}

/* I1175: the text of Sources page row `row` (mui_srcs_row: name, On or Off, Local or the shared sync status). */
static const char* mui_src_row(void* ctx, long row) {
    mui_state* s = (mui_state*)ctx;
    if (row < 0 || (uint32_t)row >= s->srcs.n) {
        return "";
    }
    mui_srcs_row(&s->srcs, (uint32_t)row, s->src_rowbuf, sizeof s->src_rowbuf);
    return s->src_rowbuf;
}

/* I1175: the Sources page (desktop PG source page): one row per source with its switch and sync status, a Switch and a Sync button. */
static Object* mui_sources_page_group(mui_state* s) {
    mui_aalist_spec sp;
    Object* lg;
    Object* bar = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    Object* g = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
    memset(&sp, 0, sizeof sp);
    sp.active = -1;
    sp.row = mui_src_row;
    sp.ctx = s;
    sp.max_rows = (int)SEL_SOURCE_MAX;
    sp.double_pick = 0;
    lg = mui_aalist_new(&sp, &s->src_list);
    s->src_switch = mui_make_button_w("Switch On or Off", "tile", CTL_SRC_SWITCH, 0);
    s->src_sync = mui_make_button_w("Sync", "tile", CTL_SRC_SYNC, 0);
    if (g == NULL || bar == NULL || lg == NULL || s->src_list == NULL || s->src_switch == NULL || s->src_sync == NULL) {
        return NULL;
    }
    DoMethod(s->src_list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_SRC_LIST);
    DoMethod(bar, OM_ADDMEMBER, s->src_switch);
    DoMethod(bar, OM_ADDMEMBER, s->src_sync);
    DoMethod(bar, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    DoMethod(g, OM_ADDMEMBER, lg);
    DoMethod(g, OM_ADDMEMBER, bar);
    return g;
}

/* I1175: the Library mode row: the shared mode names (Composers, Tree, Columns, Files, History, Sources) as one Cycle. */
static Object* mui_mode_row_group(mui_state* s) {
    uint32_t k;
    s->mode_row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_ShowMe, FALSE, TAG_DONE);
    for (k = 0; k < MUI_LIBMODE_COUNT; k++) {
        s->libmode_ent[k] = mui_libmode_label(k);
    }
    s->libmode_ent[MUI_LIBMODE_COUNT] = NULL;
    s->cyc_libmode = mui_make_cycle(s->libmode_ent, 0, CTL_LIBMODE);
    if (s->mode_row == NULL || s->cyc_libmode == NULL) {
        return NULL;
    }
    DoMethod(s->mode_row, OM_ADDMEMBER, s->cyc_libmode);
    DoMethod(s->mode_row, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    return s->mode_row;
}

/* I1176: the Queue scope row (desktop queue page header): sel_uim_scope_label (Scope: Library, Scope: Album, Scope: Playlist, the desktop button text) as one Cycle, shown on the queue list. */
static Object* mui_scope_row_group(mui_state* s) {
    uint32_t k;
    s->scope_row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_ShowMe, FALSE, TAG_DONE);
    for (k = 0; k < MUI_SCOPE_COUNT; k++) {
        s->scope_ent[k] = sel_uim_scope_label(k);
    }
    s->scope_ent[MUI_SCOPE_COUNT] = NULL;
    s->cyc_scope = mui_make_cycle(s->scope_ent, 0, CTL_SCOPE);
    if (s->scope_row == NULL || s->cyc_scope == NULL) {
        return NULL;
    }
    DoMethod(s->scope_row, OM_ADDMEMBER, s->cyc_scope);
    DoMethod(s->scope_row, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    return s->scope_row;
}

/* I795 (P5): the Search facet chips (sel_uim_search_facets: All, Songs, Albums, Artists), shown while the list's source is Search. */
static Object* mui_facet_row_group(mui_state* s) {
    uint32_t k;
    s->facet_row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_ShowMe, FALSE, TAG_DONE);
    if (s->facet_row == NULL) {
        return NULL;
    }
    for (k = 0; k < SEL_UIM_SEARCH_FACETS; k++) {
        s->facet_btn[k] = mui_make_button_ex(sel_uim_search_facets[k], sel_uim_search_facets[k], "tile", NULL, CTL_FACET_BASE + (LONG)k, 100, 0, MUI_AA_ROLE_TOGGLE, MUI_AA_ALIGN_CENTRE, 0, 0);
        if (s->facet_btn[k] == NULL) {
            return NULL;
        }
        DoMethod(s->facet_row, OM_ADDMEMBER, s->facet_btn[k]);
    }
    DoMethod(s->facet_row, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    return s->facet_row;
}

static Object* mui_main_group(mui_state* s) {
    Object* g = mui_aapanel(MUI_AA_PANEL_PANEL, 0);
    s->col_bp = SEL_UIM_BP_COUNT;
    mui_columns_format(s->col_bp, s->col_fmt, sizeof s->col_fmt, g_pixel_aspect_q8);
    mui_main_cols_fill(s->col_bp, &s->main_cols, s->cell, s->cell_n);
    g_list_hook.h_Entry = (__typeof__(g_list_hook.h_Entry))(void (*)(void))HookEntry;
    g_list_hook.h_SubEntry = (__typeof__(g_list_hook.h_SubEntry))(void (*)(void))mui_list_display;
    s->crumb = mui_aatext(" ", MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 1);
    {
        mui_aalist_spec sp;
        Object* lg;
        memset(&sp, 0, sizeof sp);
        sp.active = -1;
        sp.cell = mui_main_cell;
        sp.ctx = s;
        sp.cols = &s->main_cols;
        sp.title = 1;
        sp.double_pick = 1;
        sp.max_rows = 12;
        lg = mui_aalist_new(&sp, &s->list); /* I667i2: AAList with columns, title row and the display hook (design 3.6) */
        if (g == NULL || lg == NULL || s->list == NULL || s->crumb == NULL) {
            return NULL;
        }
        s->listview = mui_listview_new(lg, (ULONG)CTL_SM_BASE, (ULONG)SEL_UIM_SONG_MENU_ITEMS); /* Selecta_Listview: Amiga-1..5 over the list (I558) */
    }
    if (s->listview == NULL) {
        return NULL;
    }
    set(s->listview, MUIA_ContextMenu, (ULONG)mui_song_context_menu()); /* MUI-P4-6: right mouse button */
    DoMethod(s->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_LIST_DBL);
    DoMethod(s->list, MUIM_Notify, UPA_List_Active, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_LIST_SEL);
    s->tile_step = 2u; /* I1174: the desktop's TILE_STEP_DEFAULT; no pinch on the Amiga, so the step stays until a control moves it (mui_tile_step_move) */
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
    s->state_txt = mui_aatext(" ", MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 1);
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
    s->col_page = mui_col_page_group(s);
    if (s->col_page == NULL) {
        return NULL;
    }
    DoMethod(g, OM_ADDMEMBER, s->col_page);
    s->home_page = mui_home_page_group(s);
    s->browse_page = mui_browse_page_group(s);
    s->src_page = mui_sources_page_group(s);
    if (s->home_page == NULL || s->browse_page == NULL || s->src_page == NULL || mui_facet_row_group(s) == NULL || mui_scope_row_group(s) == NULL || mui_mode_row_group(s) == NULL) {
        return NULL;
    }
    DoMethod(g, OM_ADDMEMBER, s->home_page);
    DoMethod(g, OM_ADDMEMBER, s->browse_page);
    DoMethod(g, OM_ADDMEMBER, s->src_page);
    DoMethod(g, OM_ADDMEMBER, s->mode_row);
    DoMethod(g, OM_ADDMEMBER, s->scope_row);
    DoMethod(g, OM_ADDMEMBER, s->facet_row);
    DoMethod(g, OM_ADDMEMBER, s->listview);
    /* MUI-P5-1: the canvas object sits beside the list, hidden until the style switch (MUI-P3-5) shows it. */
    {
        static const mui_grid_style grid = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 96, 8, 8};
        s->canvas = mui_canvas_new(&grid, g_screen_rtg);
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
static Object* mui_np_text(char* buf, size_t cap, const char* init, mui_label label) {
    snprintf(buf, cap, "%s", init);
    return mui_aatext(buf, mui_pens_label_tone(label), MUI_AA_ALIGN_LEFT, 0);
}

/* compact (I960c, design I949 section 6): the same text, Like, tabs and pages, built as the Now Playing main page: a heading, then the
 * art beside the four text rows and the heart, no visualizer strip (the Visualizer page owns the plugin). Full: the detail column. */
static Object* mui_detail_group(mui_state* s, int compact) {
    Object* g = mui_aapanel(MUI_AA_PANEL_PANEL, 0);
    Object* art = mui_aapanel_wh(MUI_AA_PANEL_RAISED, MUI_DETAIL_ART_PX, (int)mui_art_height(MUI_DETAIL_ART_PX, g_pixel_aspect_q8));
    Object* top = compact ? MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, TAG_DONE) : NULL;
    Object* txt = compact ? MUI_NewObject((char*)MUIC_Group, TAG_DONE) : NULL;
    Object* like = NULL;
    Object* lyr_view;
    Object* info_g;
    Object* trk_g;
    uint32_t i;
    if (g == NULL) {
        return NULL;
    }
    s->np_lyric_buf[0] = '\0';
    s->np_title = mui_np_text(s->np_buf[0], sizeof s->np_buf[0], " ", MUI_LBL_TITLE);
    s->np_artist = mui_np_text(s->np_buf[1], sizeof s->np_buf[1], " ", MUI_LBL_ARTIST);
    s->np_album = mui_np_text(s->np_buf[2], sizeof s->np_buf[2], " ", MUI_LBL_ALBUM);
    s->np_note = mui_np_text(s->np_buf[3], sizeof s->np_buf[3], " ", MUI_LBL_UPCOMING);
    {
        mui_aalist_spec sp;
        memset(&sp, 0, sizeof sp);
        sp.active = -1;
        sp.row = mui_lyric_row;
        sp.ctx = s;
        sp.max_rows = 3;
        sp.no_select = 1; /* lyrics: one column, no selection, wheel only (design 3.6) */
        lyr_view = mui_aalist_new(&sp, &s->np_lyrics);
    }
    s->info_ptr[0] = NULL;
    s->trk_ptr[0] = NULL;
    info_g = mui_string_aalist(s->info_ptr, 0, 6, &s->info_list);
    trk_g = mui_string_aalist(s->trk_ptr, 0, 8, &s->trk_list);
    s->trk_group = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
    /* I1125 (design I973 6a Q6): the Like heart sits on the Now Playing panel like the Mac np_like, not in the player bar. */
    like = mui_make_button_icon("Like", "Like", CTL_P4_BASE + (LONG)P4_LIKE);
    if (art != NULL) {
        DoMethod(art, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE)); /* a group needs one child */
    }
    if (compact && top != NULL && txt != NULL) {
        DoMethod(g, OM_ADDMEMBER, mui_aatext("Now Playing", mui_pens_label_tone(MUI_LBL_SECTION), MUI_AA_ALIGN_LEFT, 0));
        if (art != NULL) {
            DoMethod(top, OM_ADDMEMBER, art);
        }
        DoMethod(txt, OM_ADDMEMBER, s->np_title);
        DoMethod(txt, OM_ADDMEMBER, s->np_artist);
        DoMethod(txt, OM_ADDMEMBER, s->np_album);
        DoMethod(txt, OM_ADDMEMBER, s->np_note);
        DoMethod(top, OM_ADDMEMBER, txt);
        if (like != NULL) {
            DoMethod(top, OM_ADDMEMBER, like);
        }
        DoMethod(g, OM_ADDMEMBER, top);
    } else {
        if (art != NULL) {
            DoMethod(g, OM_ADDMEMBER, art);
        }
        DoMethod(g, OM_ADDMEMBER, s->np_title);
        DoMethod(g, OM_ADDMEMBER, s->np_artist);
        DoMethod(g, OM_ADDMEMBER, s->np_album);
        DoMethod(g, OM_ADDMEMBER, s->np_note);
        if (like != NULL) {
            DoMethod(g, OM_ADDMEMBER, like);
        }
    }
    if (!compact && s->vizset.n > 0u) {
        s->cyc_viz = mui_make_cycle(s->vizset.name_, (LONG)s->vizset.sel, CTL_P4_BASE + P4_VIZPICK);
        s->viz_area = mui_vizarea_new(&s->vizset, g_tokens, g_screen_rtg, g_pixel_aspect_q8);
        if (s->cyc_viz != NULL && s->viz_area != NULL) {
            DoMethod(g, OM_ADDMEMBER, s->viz_area);
            DoMethod(g, OM_ADDMEMBER, s->cyc_viz);
        }
    }
    /* I667j (design 3.9): the detail tabs. Lyrics (with the upcoming queue), Info and Tracks are the pages of one page Group, switched
     * by an AATabs strip (a notification from UPA_Tabs_Active to MUIA_Group_ActivePage), instead of one tall stack. */
    {
        static const char* const tab_names[] = {"Lyrics", "Info", "Tracks", NULL};
        mui_aatabs_spec ts;
        Object* tabs;
        Object* pages = MUI_NewObject((char*)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, TAG_DONE);
        Object* page_lyr = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
        Object* page_info = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
        Object* page_trk = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
        ts.captions = tab_names;
        ts.active = 0;
        tabs = mui_aatabs_new(&ts);
        s->np_tabs = tabs;
        if (pages != NULL && page_lyr != NULL && page_info != NULL && page_trk != NULL && tabs != NULL) {
            if (lyr_view != NULL) {
                DoMethod(page_lyr, OM_ADDMEMBER, lyr_view);
            }
            DoMethod(page_lyr, OM_ADDMEMBER, mui_aatext("Next in Queue", mui_pens_label_tone(MUI_LBL_SECTION), MUI_AA_ALIGN_CENTRE, 0));
            for (i = 0; i < 3; i++) {
                s->np_up[i] = mui_np_text(s->np_up_buf[i], sizeof s->np_up_buf[i], " ", MUI_LBL_UPCOMING);
                DoMethod(page_lyr, OM_ADDMEMBER, s->np_up[i]);
            }
            /* I1313d: players this machine cannot run, one row per line the registry filter wrote (st->players_off, never derived
             * here); the box stays hidden until a line arrives, as the text UI leaves the section out. */
            s->np_off_box = MUI_NewObject((char*)MUIC_Group, MUIA_ShowMe, FALSE, TAG_DONE);
            if (s->np_off_box != NULL) {
                DoMethod(s->np_off_box, OM_ADDMEMBER,
                         mui_aatext("Players Turned Off", mui_pens_label_tone(MUI_LBL_SECTION), MUI_AA_ALIGN_CENTRE, 0));
                for (i = 0; i < MUI_OFF_ROWS; i++) {
                    s->np_off[i] = mui_np_text(s->np_off_buf[i], sizeof s->np_off_buf[i], " ", MUI_LBL_UPCOMING);
                    DoMethod(s->np_off_box, OM_ADDMEMBER, s->np_off[i]);
                }
                DoMethod(page_lyr, OM_ADDMEMBER, s->np_off_box);
            }
            if (info_g != NULL) {
                DoMethod(page_info, OM_ADDMEMBER, info_g);
            } else {
                DoMethod(page_info, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
            }
            {
                /* I1178: the desktop Song Information flag buttons (Mark Wrong, Skip Song, Save Note); label and line from sel_uim_flag_*,
                 * the note comes from the command field as Rename's name does. */
                Object* fr = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, TAG_DONE);
                uint32_t fk;
                for (fk = 0; fr != NULL && fk < 3u; fk++) {
                    const char* fl = mui_viewbtn_label(2u + fk);
                    Object* fb = mui_make_button_i(fl, fl, NULL, NULL, CTL_P4_BASE + (LONG)(P4_SIWRONG + fk), 100, 0, MUI_AA_ROLE_NORMAL);
                    if (fb != NULL) {
                        DoMethod(fr, OM_ADDMEMBER, fb);
                    }
                }
                if (fr != NULL) {
                    DoMethod(page_info, OM_ADDMEMBER, fr);
                }
            }
            if (s->trk_group != NULL && trk_g != NULL) {
                DoMethod(s->trk_group, OM_ADDMEMBER, trk_g);
                DoMethod(page_trk, OM_ADDMEMBER, s->trk_group);
            } else {
                DoMethod(page_trk, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
            }
            DoMethod(pages, OM_ADDMEMBER, page_lyr);
            DoMethod(pages, OM_ADDMEMBER, page_info);
            DoMethod(pages, OM_ADDMEMBER, page_trk);
            DoMethod(tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, pages, 3, MUIM_Set, MUIA_Group_ActivePage, MUIV_TriggerValue);
            DoMethod(g, OM_ADDMEMBER, tabs);
            DoMethod(g, OM_ADDMEMBER, pages);
        }
    }
    return g;
}

/* I918: shows the tabs when every full plugin name fits the width of the page area (sel_uim_viz_tabs_fit, the rule the desktop UI uses),
 * else the cycle. Measured with the window font; called each wait turn while the page is shown, acts only on a change. */
static void mui_viz_chooser_update(mui_state* s) {
    int32_t w[MUI_VIZ_PLUGINS];
    uint32_t k;
    int on;
    if (s->tabs_viz_page == NULL || s->viz_page_area == NULL || _rp(s->viz_page_area) == NULL || _mwidth(s->viz_page_area) <= 0) {
        return;
    }
    for (k = 0; k < s->vizset.n && k < MUI_VIZ_PLUGINS; k++) {
        w[k] = (int32_t)TextLength(_rp(s->viz_page_area), (STRPTR)s->vizset.name_[k], (ULONG)strlen(s->vizset.name_[k])) + 24;
    }
    on = sel_uim_viz_tabs_fit(w, s->vizset.n, 4, (int32_t)_mwidth(s->viz_page_area));
    if (on != s->viz_tabs_on) {
        s->viz_tabs_on = on;
        DoMethod(s->main_pages, MUIM_Group_InitChange);
        set(s->tabs_viz_page, MUIA_ShowMe, (ULONG)on);
        set(s->cyc_viz_page, MUIA_ShowMe, (ULONG)!on);
        DoMethod(s->main_pages, MUIM_Group_ExitChange);
    }
}

/* I735: the main slot as a page group (MUI 3.8 Group_PageMode): page MUI_PANE_LIST is the song list group, MUI_PANE_VIZ the Visualizer
 * page, MUI_PANE_TRACKER the Pattern page. The Visualizer page reuses Selecta_VizArea (its own instance over the same vizset, so the
 * detail strip and the page both draw the one plugin) with the plugin picker; the Pattern page is an AAList over the rows
 * mui_detail_refresh fills for the detail tab. Both pages sit in the main slot, which mui_layout_map shows at every width. */
static Object* mui_main_pages(mui_state* s, Object* np_page) {
    Object* list_g = mui_main_group(s);
    Object* pages = MUI_NewObject((char*)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, TAG_DONE);
    Object* viz_g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    Object* trk_g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    Object* rows = mui_string_aalist(s->trk_ptr, 0, 16, &s->trk_page_list);
    if (list_g == NULL || pages == NULL || viz_g == NULL || trk_g == NULL) {
        return list_g;
    }
    if (s->vizset.n > 0u) {
        s->cyc_viz_page = mui_make_cycle(s->vizset.name_, (LONG)s->vizset.sel, CTL_P4_BASE + P4_VIZPICK);
        s->viz_page_area = mui_vizarea_new(&s->vizset, g_tokens, g_screen_rtg, g_pixel_aspect_q8);
        mui_vizarea_set_tall(s->viz_page_area);
    }
    if (s->cyc_viz_page != NULL && s->viz_page_area != NULL) {
        /* I918: one chooser above the one area. Tabs with the full names when they fit the page width, else the cycle; both exist and
         * mui_viz_chooser_update shows the one the measured width allows. A tab click sets the cycle, whose notification is the one
         * control id (P4_VIZPICK), so the pick runs the same handler either way. */
        mui_aatabs_spec ts;
        ts.captions = s->vizset.name_;
        ts.active = (int)s->vizset.sel;
        s->tabs_viz_page = s->vizset.n <= MUI_AATABS_MAX ? mui_aatabs_new(&ts) : NULL;
        s->viz_tabs_on = -1;
        if (s->tabs_viz_page != NULL) {
            set(s->tabs_viz_page, MUIA_ShowMe, FALSE);
            DoMethod(s->tabs_viz_page, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, s->cyc_viz_page, 3, MUIM_Set, UPA_Cycle_Active, MUIV_TriggerValue);
            DoMethod(viz_g, OM_ADDMEMBER, s->tabs_viz_page);
        }
        DoMethod(viz_g, OM_ADDMEMBER, s->cyc_viz_page);
        DoMethod(viz_g, OM_ADDMEMBER, s->viz_page_area);
    } else {
        DoMethod(viz_g, OM_ADDMEMBER, mui_aatext("No visualizer plugin is loaded", MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 1));
        /* I1307: a lone text has a fixed maximum height; the page group takes its pages' smallest maximum and the body row
         * the smallest of its columns, so without this filler the window stops ~150 rows short of an 800x600 screen. */
        DoMethod(viz_g, OM_ADDMEMBER, MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE)); /* I1307: unbounded page height */
    }
    DoMethod(trk_g, OM_ADDMEMBER, rows != NULL ? rows : MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    DoMethod(pages, OM_ADDMEMBER, list_g);
    DoMethod(pages, OM_ADDMEMBER, viz_g);
    DoMethod(pages, OM_ADDMEMBER, trk_g);
    /* I960a/c: page MUI_PANE_NOWPLAYING is the compact Now Playing group; the full class keeps an empty page so the page index exists. */
    DoMethod(pages, OM_ADDMEMBER, np_page != NULL ? np_page : MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE));
    s->main_pages = pages;
    return pages;
}

/* The page entry: shows the pane of `page` (mui_pane_open counts it), whatever opened it (the Mac UI's key, the View menu item). */
static void mui_pane_show(mui_state* s, uint32_t page) {
    uint32_t pane = mui_pane_open(&s->pane, page);
    if (pane == MUI_PANE_NOWPLAYING) {
        s->p4[P4_NPPAGE]++; /* I960d sentinel: the Now Playing page was shown */
    }
    if (s->main_pages != NULL) {
        set(s->main_pages, MUIA_Group_ActivePage, (ULONG)pane);
    }
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
    MUI_SIG(so->bank_missing);
    MUI_SIG(st->lyric_line);
    MUI_SIG(st->lyric_lines);
    MUI_SIG(st->queue_playing);
    MUI_SIG(st->queue_count);
    MUI_SIG(pv != NULL ? pv->seq : 0u);
    {
        const char* off;
        for (off = st->players_off; off != NULL && *off != '\0'; off++) { /* each line's bytes, then its NUL, until the empty line */
            MUI_SIG(*off);
            if (off[1] == '\0') {
                MUI_SIG(0);
                off++;
            }
        }
    }
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
    mui_text_set(s->np_title, s->np_buf[0]);
    mui_text_set(s->np_artist, s->np_buf[1]);
    mui_text_set(s->np_album, s->np_buf[2]);
    mui_flat(s->np_buf[3], sizeof s->np_buf[3], sel_uim_np_note(so) != NULL ? sel_uim_np_note(so) : " "); /* I1127c */
    mui_text_set(s->np_note, s->np_buf[3]);
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
    set(s->np_lyrics, UPA_List_Count, (ULONG)mui_text_lines(s->np_lyric_buf));
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
        mui_text_set(s->np_up[i], s->np_up_buf[i]);
    }
    /* I1313d: the registry's own "why off" lines, as the text UI shows them; rows past the last line stay blank. */
    if (s->np_off_box != NULL) {
        const char* line = st->players_off;
        for (i = 0; i < MUI_OFF_ROWS; i++) {
            int have = line != NULL && line[0] != '\0';
            mui_flat(s->np_off_buf[i], sizeof s->np_off_buf[i], have ? line : " ");
            mui_text_set(s->np_off[i], s->np_off_buf[i]);
            line = have ? line + strlen(line) + 1u : NULL;
        }
        set(s->np_off_box, MUIA_ShowMe, st->players_off != NULL && st->players_off[0] != '\0' ? TRUE : FALSE);
    }
    if (title != NULL && title[0] != '\0') {
        s->p4[P4_NP]++;
    }
    /* Song information: one "Label: value" row per meta tag the song carries, then the subsong line. */
    if (s->info_list != NULL) {
        uint32_t tag, rows = 0;
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
        set(s->info_list, UPA_List_Count, (ULONG)rows);
        if (rows > 0u) {
            s->p4[P4_SONGINFO]++;
        }
    }
    /* Tracker pattern rows, only when the engine answered with pattern data. */
    if (s->trk_list != NULL) {
        uint32_t rows = 0;
        s->trk_ptr[0] = NULL;
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
            if (rows > 0u) {
                s->p4[P4_TRACKER]++;
            }
        }
        set(s->trk_list, UPA_List_Count, (ULONG)rows);
        if (s->trk_page_list != NULL) { /* I735: the Pattern page shows the same rows */
            set(s->trk_page_list, UPA_List_Count, (ULONG)rows);
        }
        set(s->trk_group, MUIA_ShowMe, rows > 0u ? TRUE : FALSE);
    }
}

/* I795: the joined engine lines of the top bar Browse button; also the history key of the Browse page. */
static void mui_browse_key(char* out, size_t cap) {
    uint32_t k;
    out[0] = '\0';
    for (k = 0; k < MUI_TOPBAR_BROWSE_LINES; k++) {
        size_t n = strlen(out);
        snprintf(out + n, cap - n, "%s%s", k > 0 ? ";" : "", mui_topbar_browse_lines[k]);
    }
}

/* A cheap signature of the rows the Home page shows: when it is unchanged the controls keep their captions and counts. */
static uint32_t mui_home_signature(const sel_view_state* v, const sel_route_entry* cur) {
    uint32_t h = 2166136261u, k;
    const char* p;
    h = (h ^ (cur != NULL ? cur->id + 1u : 0u)) * 16777619u;
    for (k = 0; k < v->row_n; k++) {
        h = (h ^ (v->rows[k].kind + 1u)) * 16777619u;
        for (p = v->rows[k].text != NULL ? v->rows[k].text : ""; *p != '\0'; p++) {
            h = (h ^ (uint32_t)(unsigned char)*p) * 16777619u;
        }
    }
    return h;
}

/* I795 (P1): fill the quick tiles and the shelf slots from the Home view. On a Show all page only the chosen shelf shows, with all its rows. */
static void mui_home_fill(mui_state* s, const sel_view_state* v, const sel_route_entry* cur) {
    uint32_t k, sig = mui_home_signature(v, cur);
    int on_shelf = cur != NULL && cur->page == (uint32_t)SEL_ROUTE_PAGE_SHELF;
    if (sig == s->home_sig) {
        return;
    }
    s->home_sig = sig;
    mui_home_split(v, &s->home);
    set(s->quick_row, MUIA_ShowMe, !on_shelf && s->home.quick_n > 0u ? TRUE : FALSE);
    for (k = 0; k < MUI_HOME_QUICK_MAX; k++) {
        int on = !on_shelf && k < s->home.quick_n;
        if (on) {
            const char* text = v->rows[s->home.quick_first + k].text;
            mui_caption_set(s->quick_btn[k], text != NULL ? text : " ");
        }
        set(s->quick_btn[k], MUIA_ShowMe, on ? TRUE : FALSE);
    }
    for (k = 0; k < MUI_HOME_SHELF_MAX; k++) {
        mui_shelf sh;
        int on = 0;
        memset(&sh, 0, sizeof sh);
        if (on_shelf) {
            on = k == 0u && mui_shelf_find(v, cur->id, &sh) != 0u;
        } else if (k < s->home.shelf_n) {
            sh = s->home.shelf[k];
            sh.count = sh.count > MUI_HOME_SHELF_ROWS ? MUI_HOME_SHELF_ROWS : sh.count;
            on = 1;
        }
        if (on) {
            const char* text = v->rows[sh.heading].text;
            s->shelf_first[k] = sh.first;
            s->shelf_id[k] = sh.id;
            snprintf(s->shelf_head_buf[k], sizeof s->shelf_head_buf[k], "%s", text != NULL ? text : " ");
            mui_text_set(s->shelf_head[k], s->shelf_head_buf[k]);
            SetAttrs(s->shelf_list[k], UPA_List_Count, (ULONG)sh.count, UPA_List_Active, (ULONG)-1L, TAG_DONE);
        }
        set(s->shelf_all[k], MUIA_ShowMe, on && !on_shelf ? TRUE : FALSE);
        set(s->shelf_grp[k], MUIA_ShowMe, on ? TRUE : FALSE);
    }
}

/* I795 (P4): the Browse tiles from the engine's format and composer lists. */
static void mui_browse_fill(mui_state* s, const sel_state* st) {
    uint32_t k, sig = (uint32_t)(st->formats != NULL) + 2u * (uint32_t)(st->composers != NULL);
    mui_btile t[MUI_TILE_MAX];
    uint32_t n = mui_browse_tiles(st, t, MUI_TILE_MAX);
    for (k = 0; k < n; k++) {
        sig = sig * 31u + t[k].count + (uint32_t)(unsigned char)t[k].label[0];
    }
    if (sig == s->tile_sig && n == s->tile_n) {
        return;
    }
    s->tile_sig = sig;
    s->tile_n = n;
    memcpy(s->tile, t, sizeof s->tile);
    for (k = 0; k < MUI_TILE_MAX; k++) {
        if (k < n) {
            char cap[160];
            mui_tile_caption(&t[k], cap, sizeof cap);
            mui_caption_set(s->tile_btn[k], cap);
        }
        set(s->tile_btn[k], MUIA_ShowMe, k < n ? TRUE : FALSE);
    }
}

/* The page that shows in the main panel: the collection page (P3), the Home page and Show all page (P1), the Browse tile page (P4), else
 * the main list; and the Search facet chips (P5) above the list while its source is Search. */
static void mui_pages_sync(mui_state* s, const sel_state* st, const sel_route_entry* cur, int col) {
    const sel_view_state* v = st->view;
    char key[128];
    int home = 0, browse = 0, facet = 0, srcp = 0, modes = 0, scope = 0;
    uint32_t scope_idx = sel_uim_scope_index(st->options[SEL_OPT_SCOPE]);
    if (v != NULL && v->source == SEL_VIEW_HOME && (cur == NULL || cur->page == (uint32_t)SEL_ROUTE_PAGE_HOME || cur->page == (uint32_t)SEL_ROUTE_PAGE_SHELF)) {
        home = 1;
    }
    mui_browse_key(key, sizeof key);
    if (cur != NULL && cur->page >= (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE && strcmp(cur->key, key) == 0) {
        browse = 1;
    }
    if (cur != NULL && cur->page >= (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE && strncmp(cur->key, "SOURCES", 7) == 0 && (cur->key[7] == '\0' || cur->key[7] == ';')) {
        srcp = 1; /* I1175: the Sources tile and the Sources mode both enter this page */
    }
    modes = !col && !home && !browse;
    if (v != NULL && v->source == SEL_VIEW_SEARCH && !col) {
        facet = 1;
    }
    scope = v != NULL && v->source == SEL_VIEW_QUEUE && !col && !home && !browse && !srcp; /* I1176 */
    if (scope && scope_idx != s->scope_seen) {
        s->scope_seen = scope_idx; /* the engine option is the truth; the Cycle follows it */
        set(s->cyc_scope, UPA_Cycle_Active, (ULONG)scope_idx);
    }
    if (home) {
        mui_home_fill(s, v, cur);
    }
    if (browse) {
        mui_browse_fill(s, st);
    }
    if (srcp && s->src_gen_seen != s->srcs.gen) {
        s->src_gen_seen = s->srcs.gen;
        SetAttrs(s->src_list, UPA_List_Count, (ULONG)s->srcs.n, TAG_DONE);
    }
    if (col != s->col_shown || home != s->home_shown || browse != s->browse_shown || facet != s->facet_shown || srcp != s->src_shown || modes != s->modes_shown || scope != s->scope_shown) {
        ULONG parent = 0;
        uint32_t k;
        s->col_shown = col;
        s->home_shown = home;
        s->browse_shown = browse;
        s->facet_shown = facet;
        s->src_shown = srcp;
        s->modes_shown = modes;
        s->scope_shown = scope;
        GetAttr(MUIA_Parent, s->col_page, &parent);
        if (parent != 0) {
            DoMethod((Object*)parent, MUIM_Group_InitChange);
        }
        set(s->col_page, MUIA_ShowMe, col ? TRUE : FALSE);
        set(s->home_page, MUIA_ShowMe, home ? TRUE : FALSE);
        set(s->browse_page, MUIA_ShowMe, browse ? TRUE : FALSE);
        set(s->facet_row, MUIA_ShowMe, facet ? TRUE : FALSE);
        set(s->src_page, MUIA_ShowMe, srcp ? TRUE : FALSE);
        set(s->mode_row, MUIA_ShowMe, modes ? TRUE : FALSE);
        set(s->scope_row, MUIA_ShowMe, scope ? TRUE : FALSE);
        set(s->listview, MUIA_ShowMe, col || home || browse || srcp ? FALSE : TRUE);
        for (k = 0; k < SEL_UIM_SEARCH_FACETS; k++) {
            mui_aabtn_set_on(s->facet_btn[k], k == s->search_facet);
        }
        if (parent != 0) {
            DoMethod((Object*)parent, MUIM_Group_ExitChange);
        }
    }
}

/* I1306b: the posted song_briefs read of the shown collection page (the text UI's model, uimodel/briefs.h). A landed answer redraws the
 * list (setting the count always redraws an AAList); a new page posts its read once. */
static void mui_col_briefs(mui_state* s, const sel_collection_page* pg, int col) {
    if (sel_uim_briefs_take(&s->col_brf, s->engine) && col && pg != NULL && pg->serial == s->col_brf.serial) {
        SetAttrs(s->col_list, UPA_List_Count, (ULONG)pg->songs, TAG_DONE);
    }
    if (!col || !sel_uim_briefs_due(&s->col_brf, s->engine, pg)) {
        return;
    }
    free(s->col_brf.buf);
    s->col_brf.cap = sel_uim_briefs_cap(pg);
    s->col_brf.buf = (char*)malloc(s->col_brf.cap);
    (void)sel_uim_briefs_post(&s->col_brf, s->engine, pg);
}

/* I794: the route history drives the top bar Back / Forward state and which of the two main views shows: the collection page (P3) when
 * the history is on an album, artist or composer page the engine has built, else the main list (Home, Show all, doors). */
static void mui_route_sync(mui_state* s, const sel_state* st) {
    const sel_collection_page* pg = st->collection;
    const sel_route_entry* cur = mui_route_cur(&s->route);
    int col = cur != NULL && mui_route_on_collection(&s->route) && pg != NULL && pg->kind == cur->kind && strcmp(pg->key, cur->key) == 0;
    int lit = mui_route_lit_door(&s->route); /* I797 (S2): the door of the current page takes the SELECTED face, the others none */
    uint32_t dk;
    for (dk = 0; dk < MUI_SIDEBAR_DOORS; dk++) {
        if (s->door_btn[dk] != NULL) {
            mui_aabtn_set_on(s->door_btn[dk], (int)dk == lit);
        }
    }
    if (s->nav_back != NULL) {
        int back_off = mui_route_can(&s->route, -1) ? FALSE : TRUE, fwd_off = mui_route_can(&s->route, 1) ? FALSE : TRUE;
        if (mui_flag_commit(&s->nav_back_dis, back_off)) { /* I966: only on a change */
            set(s->nav_back, MUIA_Disabled, back_off);
        }
        if (mui_flag_commit(&s->nav_fwd_dis, fwd_off)) {
            set(s->nav_fwd, MUIA_Disabled, fwd_off);
        }
    }
    if (col && pg->serial != s->col_serial) {
        s->col_serial = pg->serial;
        snprintf(s->col_title_buf, sizeof s->col_title_buf, "%s", pg->title);
        snprintf(s->col_meta_buf, sizeof s->col_meta_buf, "%s%s%s%s%u songs", pg->sub, pg->sub[0] != '\0' ? " - " : "", pg->year != 0u ? "" : "",
                 pg->year != 0u ? "" : "", (unsigned)pg->songs);
        if (pg->year != 0u) {
            size_t n = strlen(s->col_meta_buf);
            snprintf(s->col_meta_buf + n, sizeof s->col_meta_buf - n, " - %u", (unsigned)pg->year);
        }
        mui_text_set(s->col_title, s->col_title_buf);
        mui_text_set(s->col_meta, s->col_meta_buf);
        set(s->col_artist, MUIA_ShowMe, pg->kind == (uint32_t)SEL_LIB_ITEM_ALBUM && pg->sub[0] != '\0' ? TRUE : FALSE);
        SetAttrs(s->col_list, UPA_List_Count, (ULONG)pg->songs, UPA_List_Active, (ULONG)-1L, TAG_DONE);
    }
    mui_col_briefs(s, pg, col);
    mui_pages_sync(s, st, cur, col);
}

static void mui_palette_sync(mui_state* s);
static void mui_apply_layout(mui_state* s);

/* I1173: Full, Window, Mini, Visual (the actions layout_*; PREF_SET ui.desktop.mode). The map under the mode decides which regions show
 * (map/mui_layout.c); Visual shows the Visualizer pane in the main region and leaving it returns to the list. */
static void mui_mode_apply(mui_state* s, uint32_t mode) {
    uint32_t before = s->lay.mode;
    if (mode == before) {
        return;
    }
    s->uim.layout = mode;
    if (s->lay.width == 0u) {
        s->lay.mode = (uint8_t)mode; /* not sized yet: the first resize check builds the map under this mode */
    } else {
        mui_layout_map_wide(&s->lay, s->lay.width, s->lay.height, g_screen_rtg ? SEL_UIM_TIER_DEFAULT : SEL_UIM_TIER_BITMAP,
                            s->lay.detail_hidden, s->lay.side_wide, mode);
        mui_apply_layout(s);
    }
    if (mode == SEL_UIM_LAYOUT_VISUAL) {
        mui_pane_show(s, mui_pane_page(MUI_PANE_VIZ));
    } else if (before == SEL_UIM_LAYOUT_VISUAL) {
        mui_pane_show(s, mui_pane_page(MUI_PANE_LIST));
    }
}

/* UI_ON_EVENT: a layout change arrives as SEL_EVT_PREFS_CHANGED (key, value), like the desktop's desk_on_event. Only the request is
 * recorded here; mui_refresh applies it on the window's own turn. */
static void mui_on_event(void* state, const sel_message* evt) {
    mui_state* s = (mui_state*)state;
    uint32_t mode;
    if (s != NULL && evt != NULL && evt->id == SEL_EVT_PREFS_CHANGED && evt->argc > 1u && evt->args[0].type == SEL_ARG_STR &&
        evt->args[1].type == SEL_ARG_STR && mui_mode_from_pref(evt->args[0].v.s, evt->args[1].v.s, &mode)) {
        s->mode_pending = mode + 1u;
    }
    if (s != NULL && evt != NULL) {
        mui_srcs_event(&s->srcs, evt); /* I1175: SOURCES rows, sync completion and sync errors */
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
    if (s->mode_pending != 0u) {
        uint32_t want = s->mode_pending - 1u;
        s->mode_pending = 0u;
        mui_mode_apply(s, want);
    }
    mui_stats_sync(s); /* I1181 */
    mui_offer_window(s); /* I1182 */
    if (mui_toast_tick(&s->toast, mui_now_ms())) { /* MUI-P4-11: the toast went away after 3 s (6 s for an error) */
        snprintf(s->status_buf, sizeof s->status_buf, "Ready");
        mui_text_set(s->status, s->status_buf);
        s->p4[P4_TOAST]++;
    }
    mui_route_sync(s, st);
    s->uim.dirty |= sel_uim_update(&s->uim, st);
    if (s->viz_area != NULL || (s->viz_page_area != NULL && s->pane.pane == MUI_PANE_VIZ)) { /* I684: one visualizer step per wait turn, on the newest engine frame; I960c: the compact class has no strip area */
        sel_uim_budget vb;
        if (sel_uim_budget_for(&vb, "rgba32")) {
            /* I918: only the area on screen steps, the page area while the Visualizer page is shown, else the strip. Both stepped on
             * one turn shared one vizset tick, so the second never got a frame and the first drew behind the page. */
            Object* shown = s->viz_page_area != NULL && s->pane.pane == MUI_PANE_VIZ ? s->viz_page_area : s->viz_area;
            if (shown == s->viz_page_area) {
                mui_viz_chooser_update(s);
            }
            if (mui_vizarea_step(shown, st->viz, &vb, mui_now_ms()) != 0) {
                s->p4[P4_VIZFRAMES] = (s->viz_area != NULL ? mui_vizarea_frames(s->viz_area) : 0u) + (s->viz_page_area != NULL ? mui_vizarea_frames(s->viz_page_area) : 0u);
            }
        }
    }
    attrs = mui_dirty_attrs(sel_uim_take_dirty(&s->uim));
    f = &s->uim.frame;
    for (a = 0; a < MUI_ATTR_COUNT; a++) {
        if ((attrs & (1u << a)) == 0) {
            continue;
        }
        if (s->txt[a] != NULL) {
            const char* t = mui_attr_text(a, f);
            snprintf(s->txt_buf[a], sizeof s->txt_buf[a], "%s", t != NULL && t[0] != '\0' ? t : " ");
            mui_text_set(s->txt[a], s->txt_buf[a]);
        } else if (a == MUI_ATTR_SEEK) {
            s->seek_set = (LONG)f->progress_q16;
            set(s->seek, MUIA_Numeric_Value, s->seek_set);
        } else if (a == MUI_ATTR_VOLUME) {
            s->vol_set = (LONG)f->volume;
            set(s->vol, MUIA_Numeric_Value, s->vol_set);
        }
    }
    for (i = 0; i < MUI_BTN_COUNT; i++) {
        const char* label = mui_transport_label(i, f);
        if (label != NULL && label != s->btn_label[i]) {
            s->btn_label[i] = label;
            if (s->btn[i] != NULL) {
                const char* act = mui_transport_action(i, f);
                mui_caption_set(s->btn[i], label);
                mui_aabtn_set_on(s->btn[i], mui_transport_on((uint32_t)i, f));
                mui_aabtn_set_alt(s->btn[i], i == MUI_BTN_PLAY && act != NULL && strcmp(act, "pause") == 0);
                mui_set_tip(s, MUI_TS_BTN + i, s->btn[i], MUI_TIP_TRANSPORT, i, f, 0); /* set_caption replaced the ShortHelp */
            }
        }
    }
    mui_subsong_sync(s, f);
    mui_palette_sync(s);
    mui_detail_refresh(s, st);
    v = st->view;
    if (v != NULL) {
        int32_t active = -1;
        uint32_t n = mui_list_build(mui_list_backend_pick(0, 0), s->entries, MUI_ROWS_MAX, v, &active);
        uint32_t packed = mui_style_kind(v->style) != MUI_STYLE_LIST;
        /* The pending click holds a window position (mui_control_fire maps a packed row back), so it resolves before packing. */
        active = mui_list_pending_resolve(&s->list_pending, active);
        if (packed) { /* I1174: Grid, Cover Flow, Compact: pack the window into List rows; a click maps back through mui_style_row_index */
            static mui_list_entry raw[MUI_ROWS_MAX];
            uint32_t chars = s->lay.width / 8u;
            if (s->lay.weight_w[MUI_SLOT_SIDEBAR] + s->lay.weight_w[MUI_SLOT_MAIN] + s->lay.weight_w[MUI_SLOT_DETAIL] > 0u) {
                chars = chars * s->lay.weight_w[MUI_SLOT_MAIN] /
                        (s->lay.weight_w[MUI_SLOT_SIDEBAR] + s->lay.weight_w[MUI_SLOT_MAIN] + s->lay.weight_w[MUI_SLOT_DETAIL]);
            }
            memcpy(raw, s->entries, n * sizeof raw[0]);
            s->style_kind = mui_style_kind(v->style);
            s->style_cursor = active;
            s->style_cols = mui_style_grid_cols(chars, s->tile_step);
            n = mui_style_pack(v->style, s->style_cols, mui_style_cell_chars(s->tile_step), raw, n, active, s->entries, MUI_ROWS_MAX);
            active = mui_style_packed_cursor(v->style, s->style_cols, active);
        } else {
            s->style_kind = MUI_STYLE_LIST;
        }
        uint32_t sig = 2166136261u ^ v->window_start ^ (n << 20) ^ ((uint32_t)(active + 1) << 8) ^ (s->lay.breakpoint << 28) ^ (st->song.duration_ms * 2654435761u) ^ st->song.queue_id;
        const char* c;
        if (s->lay.breakpoint != s->col_bp && s->lay.breakpoint < SEL_UIM_BP_COUNT) { /* MUI-P3-4: another column set */
            s->col_bp = s->lay.breakpoint;
            mui_columns_format(s->col_bp, s->col_fmt, sizeof s->col_fmt, g_pixel_aspect_q8);
            mui_main_cols_push(s);
        }
        {
            uint32_t drill = v->style == SEL_VIEW_STYLE_DRILLDOWN; /* MUI-P3-3: the breadcrumb line, drilldown only */
            char crumb[sizeof s->crumb_buf];
            mui_list_breadcrumb(v, crumb, sizeof crumb);
            if (drill && strcmp(crumb, s->crumb_buf) != 0) {
                snprintf(s->crumb_buf, sizeof s->crumb_buf, "%s", crumb);
                mui_text_set(s->crumb, s->crumb_buf);
            }
            if (mui_flag_commit(&s->crumb_show, drill)) { /* I966: only on a change */
                set(s->crumb, MUIA_ShowMe, drill ? TRUE : FALSE);
            }
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
                        mui_caption_set(s->crumb_btn[k], s->crumb_lbl[k]);
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
                mui_text_set(s->state_txt, (s->state_buf[0] != '\0' ? s->state_buf : " "));
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
            long playing = -1; /* the row whose title draws in ACCENT */
            s->list_sig = sig;
            for (i = 0; i < n; i++) {
                snprintf(s->cell[i].title, sizeof s->cell[i].title, "%s", s->entries[i].text);
                snprintf(s->cell[i].artist, sizeof s->cell[i].artist, "%s", packed ? "" : mui_row_cell(&v->rows[i], MUI_COL_ARTIST));
                if (packed) {
                    memset(&s->cell[i].extra, 0, sizeof s->cell[i].extra);
                } else {
                    char rpath[256], grp[64];
                    grp[0] = '\0';
                    if (sel_engine_view_row_path(s->engine, i, rpath, sizeof rpath)) {
                        (void)sel_engine_song_group(s->engine, rpath, grp, sizeof grp);
                    }
                    mui_row_extra_fill(st, &v->rows[i], grp, &s->cell[i].extra);
                }
                if (!packed && v->rows[i].kind == SEL_ROW_QUEUE && st->song.queue_id != 0u && v->rows[i].id == st->song.queue_id) {
                    playing = (long)i;
                }
                s->entry_ptr[i] = (const char*)&s->cell[i];
            }
            s->entry_ptr[n] = NULL;
            s->cell_n = (uint32_t)n;
            mui_main_cols_push(s); /* I1079d: a column empty for the whole list drops out */
            s->list_active_set = active;
            SetAttrs(s->list, UPA_List_Count, (ULONG)n, UPA_List_Playing, (ULONG)playing, UPA_List_Active, (ULONG)(active < 0 ? -1L : (long)active), TAG_DONE);
            mui_ctx_sync(v); /* I1179: the context menu follows the cursor row */
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
    mui_text_set(s->status, s->status_buf);
}

/* I1181: the Library Statistics window, label and value per row (the desktop panel's two columns) and a More / Less button. Built on
 * the first stats_show, then only opened or closed; mui_stats_sync fills the rows from the engine's stats (sel_state.stats). */
static void mui_stats_sync(mui_state* s) {
    const sel_state* st = sel_engine_state(s->engine);
    uint32_t i;
    ULONG open = 0;
    if (s->stats_win == NULL) {
        return;
    }
    GetAttr(MUIA_Window_Open, s->stats_win, &open);
    if (s->stats_panel.open && !open && mui_engine_line(s, mui_menu_line("stats_hide"))) {
        mui_stats_menu(&s->stats_panel, "stats_hide"); /* closed with its close button: tell the engine, as the desktop does on leaving */
    }
    for (i = 0; i < MUI_STATS_MAX; i++) {
        const char *l = "", *v = "";
        if (st == NULL || mui_stats_panel_row(&s->stats_panel, st->stats, i, &l, &v) == 0u) {
            l = v = "";
        }
        mui_aatext_set(s->stats_lbl[i], l);
        mui_aatext_set(s->stats_val[i], v);
    }
    mui_aabtn_set_caption(s->stats_more, mui_stats_more_label(&s->stats_panel));
}

static void mui_stats_window(mui_state* s) {
    uint32_t i;
    if (!s->stats_panel.open) {
        if (s->stats_win != NULL) {
            set(s->stats_win, MUIA_Window_Open, FALSE);
        }
        return;
    }
    if (s->stats_win == NULL) {
        Object *grid = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Columns, 2, TAG_DONE), *root = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
        if (grid == NULL || root == NULL) {
            return;
        }
        for (i = 0; i < MUI_STATS_MAX; i++) {
            s->stats_lbl[i] = mui_aatext_sized("", "Total songs in the library", MUI_AA_TONE_MUTED, MUI_AA_ALIGN_LEFT, 0);
            s->stats_val[i] = mui_aatext_sized("", "0000000", MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_RIGHT, 0);
            DoMethod(grid, OM_ADDMEMBER, s->stats_lbl[i]);
            DoMethod(grid, OM_ADDMEMBER, s->stats_val[i]);
        }
        s->stats_more = mui_make_button_i("More", "Less", NULL, NULL, CTL_STATS_MORE, 0, 0, MUI_AA_ROLE_NORMAL);
        DoMethod(root, OM_ADDMEMBER, grid);
        DoMethod(root, OM_ADDMEMBER, s->stats_more);
        s->stats_win = mui_dialog_window("Library Statistics", 0x53544154UL, root);
        if (s->stats_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->stats_win);
        mui_close_on_request(s->stats_win);
    }
    set(s->stats_win, MUIA_Window_Open, TRUE);
    mui_stats_sync(s);
}

/* I1182: the first-start sync offer window. Opens once per launch when the engine marks a source due (mui_offer_panel_follow): the
 * title, one row per listed source, Never, Not now and Sync now (I1308c: the desktop's three). Built when it opens (once).
 * I1308: the content sits in a raised card (AAPanel RAISED, the SURFACE_RAISED face the desktop nag dialog draws), whose padding and
 * spacing keep the rows apart; each row is a real check (mui_make_check, the Settings AACheck) beside its label, as the desktop
 * nagdlg_draw tick box + label. The rows were ghost buttons captioned "[x] text": free-floating framed buttons with a fake check. */
static void mui_offer_window(mui_state* s) {
    uint32_t k;
    char row[160];
    if (!s->offer_asked) {
        /* I1308e: asked on the first turn after open, never from the open's own first refresh. The shell's start-up lines after
         * the UIs open (LIBRARY_BROWSE, VIEW_SHOW, option-word PREF_SET; apps/selecta/main.c send_line) pump with no sink, so a
         * SOURCES reply queued during open was dropped, m->wait never cleared and the offer never opened. Same turn as the
         * desktop UI (desktop_ui.c, "asked on the first turn, when the engine answers UI events"). */
        if (s->opens == 0u) { /* I1308e: SOURCES waits for the first turn after open */
            return;
        }
        s->offer_asked = 1;
        (void)mui_route_send(s, "SOURCES");
        return;
    }
    if (s->offer_win == NULL && mui_offer_panel_follow(&s->offer, 0, &s->srcs)) {
        Object *root = mui_aapanel(MUI_AA_PANEL_RAISED, 0), *bar = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
        if (root == NULL || bar == NULL) {
            return;
        }
        mui_offer_panel_title(&s->offer, row, sizeof row);
        DoMethod(root, OM_ADDMEMBER, mui_aatext_sized(row, row, MUI_AA_TONE_TITLE, MUI_AA_ALIGN_LEFT, 0));
        for (k = 0; k < s->offer.o.n; k++) {
            Object* line = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
            if (line == NULL) {
                return;
            }
            mui_offer_panel_row(&s->offer, &s->srcs, k, row, sizeof row);
            s->offer_row[k] = mui_make_check(row, sel_uim_offer_ticked(&s->offer.o, k), CTL_OFFER_ROW + (LONG)k);
            DoMethod(line, OM_ADDMEMBER, s->offer_row[k]);
            DoMethod(line, OM_ADDMEMBER, mui_aatext(row, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 0));
            DoMethod(root, OM_ADDMEMBER, line);
        }
        /* I1308c: Never / Not now / Sync now, the desktop nagdlg_draw buttons in its order, in the one bar (same gaps) */
        DoMethod(bar, OM_ADDMEMBER, mui_make_button_i("Never", "Never", NULL, NULL, CTL_OFFER_NEVER, 0, 0, MUI_AA_ROLE_NORMAL));
        DoMethod(bar, OM_ADDMEMBER, mui_make_button_i("Not now", "Not now", NULL, NULL, CTL_OFFER_NOTNOW, 0, 0, MUI_AA_ROLE_NORMAL));
        DoMethod(bar, OM_ADDMEMBER, mui_make_button_i("Sync now", "Sync now", NULL, NULL, CTL_OFFER_SYNC, 0, 0, MUI_AA_ROLE_NORMAL));
        DoMethod(root, OM_ADDMEMBER, bar);
        s->offer_win = mui_dialog_window("Sync your sources", 0x53594E43UL, root);
        if (s->offer_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->offer_win);
        mui_close_on_request(s->offer_win);
        set(s->offer_win, MUIA_Window_Open, TRUE);
    }
    if (s->offer_win == NULL) {
        return;
    }
    if (s->offer.open) {
        ULONG open = 0;
        GetAttr(MUIA_Window_Open, s->offer_win, &open);
        if (!open) { /* closed with its close gadget: the same as Not now */
            (void)mui_offer_panel_not_now(&s->offer, &s->srcs, (int64_t)time(NULL), mui_route_send, s);
            return;
        }
        for (k = 0; k < s->offer.o.n; k++) { /* the check follows the model; no notify, so the set does not return the row id again */
            ULONG on = 0;
            GetAttr(MUIA_Selected, s->offer_row[k], &on);
            if ((on != 0) != (sel_uim_offer_ticked(&s->offer.o, k) != 0)) {
                SetAttrs(s->offer_row[k], MUIA_NoNotify, TRUE, MUIA_Selected, (ULONG)(sel_uim_offer_ticked(&s->offer.o, k) != 0), TAG_DONE);
            }
        }
    } else {
        set(s->offer_win, MUIA_Window_Open, FALSE);
    }
}

/* MUI-P4-14: the Help window, one List row per action table row (mui_help_row). Built once, then only opened. */
static void mui_help_open(mui_state* s) {
    uint32_t i, n = mui_help_count();
    Object *lst, *lst_g;
    if (s->help_win == NULL) {
        if (n > MUI_HELP_MAX) {
            n = MUI_HELP_MAX;
        }
        for (i = 0; i < n; i++) {
            mui_help_row(i, s->help_buf[i], sizeof s->help_buf[i]);
            s->help_ptr[i] = s->help_buf[i];
        }
        s->help_ptr[n] = NULL;
        lst_g = mui_string_aalist(s->help_ptr, (long)n, 16, &lst);
        if (lst_g == NULL || lst == NULL) {
            return;
        }
        s->help_win = mui_dialog_window("Selecta Help", 0x48454C50UL, lst_g);
        if (s->help_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->help_win);
        mui_close_on_request(s->help_win);
    }
    set(s->help_win, MUIA_Window_Open, TRUE);
}

static int mui_cmd_send(void* ctx, const char* line) {
    return mui_engine_line((mui_state*)ctx, line);
}

/* I798 (O1): the command palette window: a String over an AAList of the shared txt_palette matches (the Mac palette rows). */
static void mui_palette_show(mui_state* s) {
    mui_palette_open(&s->pal);
    if (s->pal_win == NULL) {
        Object *lst_g, *str_g;
        lst_g = mui_row_aalist(mui_palette_row, &s->pal, (long)s->pal.count, 14, &s->pal_list);
        s->pal_str = mui_make_string(CTL_PAL_STR, 0, "Search commands");
        str_g = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
        if (lst_g == NULL || s->pal_list == NULL || s->pal_str == NULL || str_g == NULL) {
            return;
        }
        DoMethod(str_g, OM_ADDMEMBER, s->pal_str);
        DoMethod(str_g, OM_ADDMEMBER, lst_g);
        s->pal_win = mui_dialog_window("Selecta Command Palette", 0x50414C54UL, str_g);
        if (s->pal_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->pal_win);
        mui_close_on_request(s->pal_win);
        DoMethod(s->pal_list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, (ULONG)CTL_PAL_LIST);
    } else {
        set(s->pal_str, MUIA_String_Contents, (ULONG) "");
        SetAttrs(s->pal_list, UPA_List_Count, (ULONG)s->pal.count, UPA_List_Active, (ULONG)0, TAG_DONE);
    }
    set(s->pal_win, MUIA_Window_Open, TRUE);
    set(s->pal_win, MUIA_Window_ActiveObject, s->pal_str);
}

/* Live filter: the String text changed, so narrow the shared palette and redraw the list (called from mui_refresh). */
static void mui_palette_sync(mui_state* s) {
    ULONG open = 0;
    char text[160];
    if (s->pal_win == NULL || s->pal_str == NULL || s->pal_list == NULL) {
        return;
    }
    GetAttr(MUIA_Window_Open, s->pal_win, &open);
    mui_str_utf8(s->pal_str, text, sizeof text); /* I1139: typed Latin-1 to UTF-8 */
    if (open != 0 && strcmp(text, s->pal.query) != 0) {
        mui_palette_filter(&s->pal, text);
        SetAttrs(s->pal_list, UPA_List_Count, (ULONG)s->pal.count, UPA_List_Active, (ULONG)0, TAG_DONE);
    }
}

/* Row `row` chosen (click or Enter): a bare verb runs, a verb with arguments fills the command line (the Mac palette_pick). */
static void mui_palette_choose(mui_state* s, long row) {
    char fill[64];
    uint32_t r;
    fill[0] = '\0';
    r = mui_palette_pick(&s->pal, row, mui_cmd_send, s, fill, sizeof fill);
    if (r == MUI_PAL_RUN || r == MUI_PAL_FILL) {
        if (r == MUI_PAL_FILL) {
            {
                char l1[sizeof fill * 2]; /* I1139 */
                set(s->str_cmd, MUIA_String_Contents, (ULONG)mui_l1(l1, sizeof l1, fill));
            }
        }
        snprintf(s->status_buf, sizeof s->status_buf, r == MUI_PAL_RUN ? "Command sent: %s" : "Command needs arguments: %s", s->pal.last_verb);
        mui_toast_show(&s->toast, s->status_buf, 0, mui_now_ms());
        set(s->pal_win, MUIA_Window_Open, FALSE);
    }
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
        act = sel_uim_pref_choice_index(r->choices, cur); /* the desktop page and the text UI share this lookup (I1188) */
        o = mui_make_cycle(s->set_entry[i], act < n ? (LONG)act : 0, id);
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {
        o = mui_make_check(r->label, cur[0] == '1', id);
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_SLIDER) {
        long lo = 0, hi = 100;
        sscanf(r->choices, "%ld,%ld", &lo, &hi);
        long v = cur[0] != '\0' ? strtol(cur, NULL, 10) : lo;
        v = v < lo ? lo : (v > hi ? hi : v); /* the desktop page clamps a stored value the same way */
        o = mui_make_slider(lo, hi, v, id, 0);
    } else {
        o = mui_make_string(id, 0, r->label);
        if (o != NULL) {
            {
                char l1[256]; /* I1139 */
                set(o, MUIA_String_Contents, (ULONG)mui_l1(l1, sizeof l1, cur));
            }
        }
    }
    return o;
}

/* I1180: the About part the desktop Settings page draws under its rows (I706d): the heading, then one block per bundled module that
 * credits its upstream (title, copyright, licence). The scan is host only, so on the Amiga the list is empty and only the heading
 * shows, as on the desktop page. Returns the vertical group of the rows grid and the About group. */
static Object* mui_settings_with_about(Object* grid) {
    sel_uim_about about;
    Object *col = MUI_NewObject((char*)MUIC_Group, TAG_DONE), *ab;
    uint32_t i;
    if (col == NULL) {
        return NULL;
    }
    sel_uim_about_load(&about);
    DoMethod(col, OM_ADDMEMBER, grid);
    ab = MUI_NewObject((char*)MUIC_Group, TAG_DONE);
    if (ab == NULL) {
        return col;
    }
    DoMethod(ab, OM_ADDMEMBER, mui_aatext(sel_uim_about_heading(), MUI_AA_TONE_TITLE, MUI_AA_ALIGN_LEFT, 0));
    if (about.count > 0u) {
        DoMethod(ab, OM_ADDMEMBER, mui_aatext(sel_uim_about_credits_heading(), MUI_AA_TONE_MUTED, MUI_AA_ALIGN_LEFT, 0));
    }
    for (i = 0; i < about.count; i++) {
        DoMethod(ab, OM_ADDMEMBER, mui_aatext(about.credit[i].title, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 0));
        DoMethod(ab, OM_ADDMEMBER, mui_aatext(about.credit[i].copyright, MUI_AA_TONE_MUTED, MUI_AA_ALIGN_LEFT, 0));
        DoMethod(ab, OM_ADDMEMBER, mui_aatext(about.credit[i].licence, MUI_AA_TONE_MUTED, MUI_AA_ALIGN_LEFT, 0));
    }
    DoMethod(col, OM_ADDMEMBER, ab);
    return col;
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
            DoMethod(grid, OM_ADDMEMBER, mui_aatext(r->label, MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_RIGHT, 0));
            DoMethod(grid, OM_ADDMEMBER, g);
        }
        grid = mui_settings_with_about(grid);
        if (grid == NULL) {
            return;
        }
        s->set_win = mui_dialog_window("Selecta Settings", 0x53455453UL, grid);
        if (s->set_win == NULL) {
            return;
        }
        DoMethod(s->app, OM_ADDMEMBER, s->set_win);
        mui_close_on_request(s->set_win);
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
        GetAttr(UPA_Cycle_Active, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%s", v < 9u && s->set_entry[i][v] != NULL ? s->set_entry[i][v] : "");
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {
        GetAttr(MUIA_Selected, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%d", v ? 1 : 0);
    } else if (r->ui == (uint32_t)SEL_UIM_PREF_SLIDER) {
        GetAttr(MUIA_Numeric_Value, s->set_ctl[i], &v);
        snprintf(val, sizeof val, "%ld", (long)v);
    } else {
        mui_str_utf8(s->set_ctl[i], val, sizeof val); /* I1139 */
    }
    sel_uim_pref_line(r->key, val, line, sizeof line);
    if (mui_engine_line(s, line)) {
        mui_p4_note(s, P4_PREF, line);
    }
}

/* I1177: the folder a new folder or playlist goes into: the engine view's cursor row on the folders source, else the root. */
static uint32_t mui_pl_target_folder(const sel_view_state* view) {
    if (view == NULL || view->source != (uint32_t)SEL_VIEW_FOLDERS || view->cursor < view->window_start) {
        return 0u;
    }
    return sel_uim_pl_target_folder(view->rows, view->row_n, (int32_t)(view->cursor - view->window_start));
}

static void mui_p4_fire(mui_state* s, uint32_t which) {
    char line[200], lines[3][160];
    const sel_view_state* view;
    const sel_view_row* row = mui_cursor_row(s, &view);
    ULONG act = 0;
    char text[160]; /* I1139: gadget contents as UTF-8 */
    uint32_t n, i;
    switch (which) {
    case P4_SEARCH:
        mui_str_utf8(s->str_search, text, sizeof text);
        n = mui_search_lines(text, lines);
        for (i = 0; i < n; i++) {
            mui_engine_line(s, lines[i]);
        }
        if (n > 0 && mui_search_all_line(text, line, sizeof line)) {
            mui_engine_line(s, line); /* I1019g: the From <source> sections, on Return only */
        }
        if (n > 0) {
            mui_p4_note(s, which, lines[0]);
        }
        break;
    case P4_CMD:
        mui_str_utf8(s->str_cmd, text, sizeof text);
        if (mui_cmdline_submit(mui_cmd_send, s, text)) {
            mui_p4_note(s, which, text);
        }
        break;
    case P4_LIKE:
        if (mui_engine_line(s, mui_like_line())) {
            mui_p4_note(s, which, mui_like_line());
        }
        break;
    case P4_RATING:
        GetAttr(UPA_Cycle_Active, s->cyc_rating, &act);
        mui_rating_line((uint32_t)act, line, sizeof line);
        if (mui_engine_line(s, line)) {
            mui_p4_note(s, which, line);
        }
        break;
    case P4_VIZPICK:
        act = s->vizset.sel;
        if (s->cyc_viz != NULL) {
            GetAttr(UPA_Cycle_Active, s->cyc_viz, &act);
        }
        if (s->cyc_viz_page != NULL && (uint32_t)act == s->vizset.sel) { /* I735: the page picker fired when the strip one did not move */
            GetAttr(UPA_Cycle_Active, s->cyc_viz_page, &act);
        }
        if ((uint32_t)act < s->vizset.n) {
            s->vizset.sel = (uint32_t)act;
            if (s->cyc_viz != NULL) {
                set(s->cyc_viz, UPA_Cycle_Active, act); /* every picker shows the plugin the engine runs */
            }
            if (s->cyc_viz_page != NULL) {
                set(s->cyc_viz_page, UPA_Cycle_Active, act);
            }
            if (s->tabs_viz_page != NULL) {
                ULONG ta = 0;
                GetAttr(UPA_Tabs_Active, s->tabs_viz_page, &ta);
                if (ta != act) { /* only on a change: the tabs' own notification sets the cycle, which must not set the tabs back */
                    set(s->tabs_viz_page, UPA_Tabs_Active, act);
                }
            }
            s->vizset.seq = 0u; /* draw the new plugin on the very next frame */
            if (mui_vizset_line(&s->vizset) != NULL && mui_engine_line(s, mui_vizset_line(&s->vizset))) {
                mui_p4_note(s, which, mui_vizset_line(&s->vizset));
            }
        }
        break;
    case P4_SUBSONG:
        GetAttr(UPA_Cycle_Active, s->cyc_subsong, &act);
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
            mui_str_utf8(s->str_cmd, text, sizeof text);
            s->sm_playlist = row->id;
            if (which == P4_POPEN && mui_playlist_page(s, row, row->text != NULL ? row->text : text)) {
                mui_p4_note(s, which, "COLLECTION_OPEN");
            } else if (mui_playlist_line(which == P4_POPEN ? "open" : which == P4_PRENAME ? "rename" : "delete", row->id, text, line, sizeof line)
                && mui_engine_line(s, line)) {
                mui_p4_note(s, which, line);
            }
        }
        break;
    case P4_PNEW:
        /* The name comes from the command field (as Rename does); empty gives the default "New Playlist" (mui_viewbtn_fire). */
        mui_str_utf8(s->str_cmd, text, sizeof text);
        if (mui_viewbtn_fire(1, text, mui_pl_target_folder(view), mui_p8_send, s)) {
            mui_p4_note(s, which, "New Playlist");
        }
        break;
    case P4_PFOLDER:
        /* I1177: the desktop's New Folder (FOLDER_ADD), into the same target folder as New Playlist. */
        mui_str_utf8(s->str_cmd, text, sizeof text);
        if (mui_viewbtn_fire(0, text, mui_pl_target_folder(view), mui_p8_send, s)) {
            mui_p4_note(s, which, "New Folder");
        }
        break;
    case P4_SIWRONG:
    case P4_SISKIP:
    case P4_SINOTE:
        /* I1178: button 2 + k of the shared view buttons = sel_uim_flag_line(k): FLAG wrong, FLAG skip, NOTE (the desktop si_button_label buttons). */
        mui_str_utf8(s->str_cmd, text, sizeof text);
        if (mui_viewbtn_fire(2u + (which - P4_SIWRONG), text, 0, mui_p8_send, s)) {
            mui_p4_note(s, which, mui_viewbtn_label(2u + (which - P4_SIWRONG)));
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

/* A Home row of the engine view is activated (list double click, quick tile, shelf entry): a shelf heading opens its Show all page, a
 * library tile its collection page (I794), a song plays and an album opens (MUI-P4-1). 1 when it was one of those. */
static int mui_home_row_fire(mui_state* s, const sel_view_state* view, uint32_t v) {
    char hl[3][48];
    const sel_view_row* row = &view->rows[v];
    const char* text = row->text != NULL ? row->text : "";
    if (row->kind == SEL_ROW_HEADING && !row->quick) { /* I794 (P2): a shelf heading opens its Show all page */
        uint32_t shelf = 1, j;
        for (j = 0; j < v; j++) {
            if (view->rows[j].kind == SEL_ROW_HEADING && !view->rows[j].quick) {
                shelf++;
            }
        }
        if (mui_route_click(&s->route, SEL_ROUTE_SHOW_ALL, 0u, shelf, "", text, "", mui_route_send, s)) {
            return 1;
        }
    }
    if (row->item_kind != 0u && row->key != NULL) { /* I794 (P3): a library tile opens its collection page */
        mui_list_pending_clear(&s->list_pending);
        if (mui_route_click(&s->route, SEL_ROUTE_HOME_TILE, row->item_kind - 1u, 0u, row->key, text, "", mui_route_send, s)) {
            s->p4[P4_HOME]++;
            return 1;
        }
    }
    if (mui_home_lines(view, v, hl) == 3) {
        mui_list_pending_clear(&s->list_pending);
        mui_engine_line(s, hl[0]);
        mui_engine_line(s, hl[1]);
        mui_engine_line(s, hl[2]);
        s->p4[P4_HOME]++;
        return 1;
    }
    return 0;
}

/* I795 (P4): a Browse tile. A composer opens its collection page (BROWSE_TILE); a format searches "format:<name>"; Sources lists the sources. */
static void mui_tile_fire(mui_state* s, uint32_t k) {
    const mui_btile* t;
    if (k >= s->tile_n) {
        return;
    }
    t = &s->tile[k];
    if (t->kind == MUI_TILE_COMPOSER) {
        mui_route_click(&s->route, SEL_ROUTE_BROWSE_TILE, (uint32_t)SEL_LIB_ITEM_COMPOSER, 0u, t->arg, t->label, "", mui_route_send, s);
    } else if (t->kind == MUI_TILE_FORMAT) {
        char q[120], lines[3][160], joined[512];
        uint32_t n, i;
        snprintf(q, sizeof q, "format:%s", t->arg);
        n = mui_search_lines(q, lines);
        joined[0] = '\0';
        for (i = 0; i < n; i++) {
            size_t len = strlen(joined);
            snprintf(joined + len, sizeof joined - len, "%s%s", i > 0u ? ";" : "", lines[i]);
        }
        mui_route_door(&s->route, (uint32_t)MUI_DOOR_MAX + 2u, joined, mui_route_send, s);
    } else {
        mui_route_door(&s->route, (uint32_t)MUI_DOOR_MAX + 3u, "SOURCES", mui_route_send, s);
    }
}

/* I669, I670: a press of a sidebar library filter control. dir -1 and +1 are the chevrons (mui_libfilter_press: the shared step and
 * clamp), dir 0 with `chip` >= 0 is a click on kind chip `chip` (clicking the one in force clears to All). Both run the shared filter pass
 * (sel_uim_lfilter_pass) into s->side_items and then rebuild S5 to S9 from the model: the model rows are the one row list. */
static void mui_sidebar_refresh(mui_state* s) {
    mui_sidebar_model m;
    Object* g = s->slot[MUI_SLOT_SIDEBAR];
    Object* fresh;
    if (s->side_rail) {
        return; /* I949c: the door rail has no Your Library block to refresh */
    }
    mui_sidebar_model_for(s, s->side_top_bar, &m);
    fresh = mui_sidebar_dyn(&m);
    if (g == NULL || fresh == NULL) {
        return;
    }
    DoMethod(g, MUIM_Group_InitChange);
    if (s->side_dyn != NULL) {
        DoMethod(g, OM_REMMEMBER, s->side_dyn);
        MUI_DisposeObject(s->side_dyn);
    }
    s->side_dyn = fresh;
    DoMethod(g, OM_ADDMEMBER, fresh);
    DoMethod(g, MUIM_Group_ExitChange);
}

static void mui_lib_press(mui_state* s, int dir, int chip) {
    char label[32];
    uint32_t n = 0u;
    if (chip >= 0) {
        uint32_t want = (uint32_t)chip + 1u == s->lib_filter ? 0u : (uint32_t)chip + 1u;
        s->lib_filter = mui_libfilter_set(want, s->engine, s->side_items, MUI_SIDEBAR_ROWS, &n, label, sizeof label);
    } else {
        s->lib_filter = mui_libfilter_press(s->lib_filter, dir, s->engine, s->side_items, MUI_SIDEBAR_ROWS, &n, label, sizeof label);
    }
    s->side_item_n = n;
    mui_sidebar_refresh(s);
}

static void mui_control_fire(mui_state* s, LONG id) {
    char line[64];
    LONG v = 0;
    if (id >= CTL_BTN_BASE && id < CTL_BTN_BASE + (LONG)MUI_BTN_COUNT) {
        if (mui_engine_line(s, mui_menu_line(mui_transport_action((uint32_t)(id - CTL_BTN_BASE), &s->uim.frame)))) {
            s->transport_hits++;
        }
    } else if (id == CTL_SUB_PREV || id == CTL_SUB_NEXT) {
        const sel_uim_frame* sf = &s->uim.frame;
        if (mui_subsong_step_line(sf->subsong, sf->subsong_count, id == CTL_SUB_PREV ? -1 : 1, line, sizeof line) && mui_engine_line(s, line)) {
            s->transport_hits++;
        }
    } else if (id >= CTL_EXTRA_BASE && id < CTL_EXTRA_BASE + (LONG)MUI_EXTRA_COUNT) {
        if (mui_action_fire(mui_extra_action((uint32_t)(id - CTL_EXTRA_BASE)), mui_p8_send, s)) {
            s->transport_hits++;
        }
    } else if (id == CTL_NAV_BACK || id == CTL_NAV_FWD) {
        mui_route_walk(&s->route, id == CTL_NAV_BACK ? -1 : 1, mui_route_send, s); /* I794 (T1) */
    } else if (id == CTL_COL_ARTIST) {
        const sel_collection_page* pg = sel_engine_state(s->engine)->collection; /* META_ARTIST: the album page's artist link */
        if (pg != NULL && pg->sub[0] != '\0') {
            mui_route_click(&s->route, SEL_ROUTE_META_ARTIST, (uint32_t)SEL_LIB_ITEM_ARTIST, 0u, pg->sub, pg->sub, "", mui_route_send, s);
        }
    } else if (id == CTL_COL_LIST) {
        const sel_collection_page* pg = sel_engine_state(s->engine)->collection;
        long picked = -1;
        GetAttr(UPA_List_Picked, s->col_list, (ULONG*)&picked);
        if (pg != NULL && pg->rows != NULL && picked >= 0 && (uint32_t)picked < pg->songs) {
            const char* r = pg->rows;
            long i;
            char pl[512];
            for (i = 0; i < picked; i++) {
                r += strlen(r) + 1u;
                r += strlen(r) + 1u;
            }
            if (mui_path_line("OPEN", r, pl, sizeof pl) != 0u && mui_engine_line(s, pl) && mui_engine_line(s, "PLAY")) {
                s->row_plays++;
            }
        }
    } else if (id > CTL_PANE_BASE + (LONG)MUI_PANE_LIST && id < CTL_PANE_BASE + (LONG)MUI_PANE_COUNT) {
        mui_pane_show(s, mui_pane_page((uint32_t)(id - CTL_PANE_BASE))); /* I735: View > Visualizer Page / Pattern Page */
    } else if (id >= CTL_QUICK_BASE && id < CTL_QUICK_BASE + (LONG)MUI_HOME_QUICK_MAX) { /* I795 (P1): a Home quick tile */
        const sel_view_state* view = sel_engine_state(s->engine)->view;
        uint32_t idx = s->home.quick_first + (uint32_t)(id - CTL_QUICK_BASE);
        if (view != NULL && idx < view->row_n) {
            mui_home_row_fire(s, view, idx);
        }
    } else if (id >= CTL_SHELF_ALL_BASE && id < CTL_SHELF_ALL_BASE + (LONG)MUI_HOME_SHELF_MAX) { /* I795 (P1): Show all of a shelf */
        LONG k = id - CTL_SHELF_ALL_BASE;
        mui_route_click(&s->route, SEL_ROUTE_SHOW_ALL, 0u, s->shelf_id[k], "", s->shelf_head_buf[k], "", mui_route_send, s);
    } else if (id >= CTL_SHELF_LIST_BASE && id < CTL_SHELF_LIST_BASE + (LONG)MUI_HOME_SHELF_MAX) { /* I795 (P1): an entry of a shelf */
        const sel_view_state* view = sel_engine_state(s->engine)->view;
        LONG k = id - CTL_SHELF_LIST_BASE;
        long picked = -1;
        GetAttr(UPA_List_Picked, s->shelf_list[k], (ULONG*)&picked);
        if (view != NULL && picked >= 0 && s->shelf_first[k] + (uint32_t)picked < view->row_n) {
            mui_home_row_fire(s, view, s->shelf_first[k] + (uint32_t)picked);
        }
    } else if (id == CTL_LIBMODE) { /* I1175: the Library mode Cycle: the shared mode lines as one route entry */
        ULONG act = 0;
        char joined[512];
        GetAttr(UPA_Cycle_Active, s->cyc_libmode, &act);
        if (mui_libmode_joined((uint32_t)act, 12u, joined, sizeof joined) > 0u) {
            mui_route_door(&s->route, (uint32_t)MUI_DOOR_MAX + 4u, joined, mui_route_send, s);
        }
    } else if (id == CTL_SCOPE) { /* I1176: SCOPE <name> then re-read the queue window (mui_scope_fire, the shared scope line) */
        ULONG act = 0;
        GetAttr(UPA_Cycle_Active, s->cyc_scope, &act);
        s->scope_seen = (uint32_t)act;
        (void)mui_scope_fire((uint32_t)act, mui_p8_send, s);
    } else if (id == CTL_SRC_LIST) {
        long picked = -1;
        GetAttr(UPA_List_Picked, s->src_list, (ULONG*)&picked);
        s->src_pick = picked;
    } else if (id == CTL_SRC_SWITCH) {
        if (s->src_pick >= 0) {
            mui_srcs_toggle(&s->srcs, (uint32_t)s->src_pick, mui_p8_send, s);
        }
    } else if (id == CTL_SRC_SYNC) {
        if (s->src_pick >= 0) {
            mui_srcs_sync(&s->srcs, (uint32_t)s->src_pick, mui_p8_send, s);
            SetAttrs(s->src_list, UPA_List_Count, (ULONG)s->srcs.n, TAG_DONE);
        }
    } else if (id >= CTL_TILE_BASE && id < CTL_TILE_BASE + (LONG)MUI_TILE_MAX) { /* I795 (P4): a Browse tile */
        mui_tile_fire(s, (uint32_t)(id - CTL_TILE_BASE));
    } else if (id >= CTL_FACET_BASE && id < CTL_FACET_BASE + (LONG)SEL_UIM_SEARCH_FACETS) { /* I795 (P5): a Search facet chip */
        const sel_view_state* view = sel_engine_state(s->engine)->view;
        char fl[40];
        uint32_t k = (uint32_t)(id - CTL_FACET_BASE);
        if (view != NULL && mui_search_facet_line(view, k, fl, sizeof fl) != 0u && mui_engine_line(s, fl)) {
            s->search_facet = k;
        }
        for (k = 0; k < SEL_UIM_SEARCH_FACETS; k++) {
            mui_aabtn_set_on(s->facet_btn[k], k == s->search_facet);
        }
    } else if (id == CTL_STATS_MORE) {
        mui_stats_more(&s->stats_panel);
    } else if (id == CTL_OFFER_NOTNOW) {
        (void)mui_offer_panel_not_now(&s->offer, &s->srcs, (int64_t)time(NULL), mui_route_send, s);
    } else if (id == CTL_OFFER_NEVER) {
        (void)mui_offer_panel_never(&s->offer, &s->srcs, mui_route_send, s);
    } else if (id == CTL_OFFER_SYNC) {
        (void)mui_offer_panel_sync(&s->offer, &s->srcs, (int64_t)time(NULL), mui_route_send, s);
    } else if (id >= CTL_OFFER_ROW && id < CTL_OFFER_ROW + (LONG)SEL_SOURCE_MAX) {
        mui_offer_panel_tick(&s->offer, (uint32_t)(id - CTL_OFFER_ROW));
    } else if (id == CTL_PAL_BTN) {
        mui_palette_show(s);
    } else if (id == CTL_PAL_LIST) {
        ULONG picked = 0;
        GetAttr(UPA_List_Picked, s->pal_list, &picked);
        mui_palette_choose(s, (long)picked);
    } else if (id == CTL_PAL_STR) {
        ULONG active = 0;
        mui_palette_sync(s);
        GetAttr(UPA_List_Active, s->pal_list, &active);
        mui_palette_choose(s, (long)active);
    } else if (id == CTL_NP_TOGGLE) {
        uint32_t hidden = mui_np_toggle(&s->lay);
        mui_apply_layout(s);
        mui_aabtn_set_alt(s->np_btn, hidden != 0u);
        mui_set_tip(s, MUI_TS_NP, s->np_btn, MUI_TIP_NP_PANEL, 0, NULL, (int)hidden);
    } else if (id == CTL_LIB_PREV || id == CTL_LIB_NEXT) {
        mui_lib_press(s, id == CTL_LIB_NEXT ? 1 : -1, -1);
    } else if (id == CTL_TOPBAR_BROWSE) {
        char joined[128]; /* I615d: what the rich top bar Browse slice sends (desk_page_load PG_BROWSE), kept as one history entry (I794) */
        mui_browse_key(joined, sizeof joined);
        mui_pane_show(s, SEL_UIM_PG_HOME); /* a list source: back to the list pane */
        mui_route_door(&s->route, (uint32_t)MUI_DOOR_MAX + 1u, joined, mui_route_send, s);
    } else if (id >= CTL_SIDE_BASE && id < CTL_SIDE_BASE + (LONG)MUI_SIDE_COUNT) {
        LONG k = id - CTL_SIDE_BASE;
        if (k == MUI_SIDE_CREATE || k == MUI_SIDE_CARD) {
            mui_viewbtn_fire(1, "", 0, mui_p8_send, s); /* S4, S9: PLAYLIST_CREATE, as the Playlists page New button */
        } else if (k >= MUI_SIDE_CHIP) {
            mui_lib_press(s, 0, (int)(k - MUI_SIDE_CHIP)); /* I669: a click on a fitting chip toggles the filter */
        } else if (k == MUI_SIDE_EXPAND) {
            mui_side_wide_toggle(&s->lay); /* I1166b: Expand / Collapse Library, the layout gives the sidebar its widened weight (desktop side_wide_toggle) */
            mui_apply_layout(s);
            mui_aabtn_set_alt(s->side_expand_btn, s->lay.side_wide != 0u);
            mui_sidebar_refresh(s);
        } else {
            mui_engine_line(s, mui_door_line(3)); /* the header and the kind chips open the playlists page (I673) */
        }
    } else if (id >= CTL_DOOR_BASE && id < CTL_DOOR_BASE + (LONG)MUI_DOOR_MAX) {
        const char* dl = mui_door_line((uint32_t)(id - CTL_DOOR_BASE)); /* the page switch: the history entry is the door's lines (I794) */
        uint32_t np_tab = 0u;
        uint32_t np_pane = mui_door_pane((uint32_t)(id - CTL_DOOR_BASE), (uint32_t)mui_screen_compact(), &np_tab);
        if (np_pane == MUI_PANE_NOWPLAYING) { /* I960d: the compact class opens its Now Playing page for the Now Playing and Song Information doors */
            mui_pane_show(s, mui_pane_page(np_pane));
            if (s->np_tabs != NULL) {
                set(s->np_tabs, UPA_Tabs_Active, (ULONG)np_tab);
            }
        } else if (dl != NULL) { /* NULL: an in-window page (Now Playing, Song Information); mui-doors checks every main-view door has a line */
            mui_pane_show(s, SEL_UIM_PG_HOME); /* the engine moves the list's source: the list pane shows it */
            mui_route_door(&s->route, (uint32_t)(id - CTL_DOOR_BASE), dl, mui_route_send, s);
        }
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
    } else if (id >= CTL_KEY_BASE && id < CTL_KEY_BASE + (LONG)mui_key_row_count()) {
        const mui_key_row* kr = mui_key_row_at((uint32_t)(id - CTL_KEY_BASE)); /* I1171: the action rows; door and pane rows send their own ids */
        if (kr != NULL && kr->kind == MUI_KEYROW_ACTION) {
            mui_action_fire(kr->id, mui_p8_send, s);
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
        GetAttr(MUIA_Numeric_Value, s->seek, (ULONG*)&v);
        if (v != s->seek_set && mui_seek_line((uint32_t)v, sel_engine_state(s->engine)->song.duration_ms, line, sizeof line)
            && mui_engine_line(s, line)) {
            s->transport_hits++;
        }
        s->seek_set = v;
    } else if (id == CTL_VOLUME) {
        GetAttr(MUIA_Numeric_Value, s->vol, (ULONG*)&v);
        if (v != s->vol_set) {
            mui_volume_line((int32_t)v, line, sizeof line);
            if (mui_engine_line(s, line)) {
                s->transport_hits++;
            }
        }
        s->vol_set = v;
    } else if (id == CTL_LIST_SEL || id == CTL_LIST_DBL) {
        const sel_view_state* view = sel_engine_state(s->engine)->view;
        GetAttr(UPA_List_Active, s->list, (ULONG*)&v);
        if (id == CTL_LIST_DBL) {
            long picked = -1;
            GetAttr(UPA_List_Picked, s->list, (ULONG*)&picked);
            v = mui_list_dbl_row((int32_t)picked, (int32_t)v);
        }
        if (view == NULL || v < 0 || (id == CTL_LIST_SEL && v == s->list_active_set)) {
            return;
        }
        s->list_active_set = v; /* the List's own row number, as mui_refresh stores it, so a repeat select of the same row is dropped */
        if (s->style_kind != MUI_STYLE_LIST) { /* I1174: a packed row stands for several window rows */
            v = mui_style_row_index(view->style, s->style_cols, (uint32_t)v, s->style_cursor);
            s->style_cursor = v;
        }
        if (id == CTL_LIST_DBL && view->style == SEL_VIEW_STYLE_TREE && (uint32_t)v < view->row_n && view->rows[v].kind == SEL_ROW_NODE) {
            /* MUI-P3-2: a node row of the tree style toggles; the engine line is VIEW_EXPAND or VIEW_COLLAPSE <node id>. */
            mui_list_pending_clear(&s->list_pending);
            snprintf(line, sizeof line, "VIEW_CURSOR %u", (unsigned)(view->window_start + (uint32_t)v));
            mui_engine_line(s, line);
            mui_list_toggle_line(&view->rows[v], line, sizeof line);
            mui_engine_line(s, line);
            return;
        }
        if (id == CTL_LIST_DBL && (uint32_t)v < view->row_n && mui_home_row_fire(s, view, (uint32_t)v)) {
            return;
        }
        if (id == CTL_LIST_DBL && (uint32_t)v < view->row_n && view->rows[v].kind == (uint32_t)SEL_ROW_PLAYLIST && view->rows[v].text != NULL &&
            mui_playlist_page(s, &view->rows[v], view->rows[v].text)) { /* I1172: the playlist's page, not the in-list open */
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
        /* I615d (design 8.2): the top bar takes the height of its children (VertWeight 0), the body the rest. */
        set(s->slot[i], MUIA_VertWeight, i == MUI_SLOT_TOPBAR ? 0 : s->lay.weight_h[i] > 0 ? s->lay.weight_h[i] : 1);
    }
    if (s->np_btn != NULL) {
        set(s->np_btn, MUIA_ShowMe, mui_np_toggle_shown(&s->lay) ? TRUE : FALSE); /* the panel exists only at three columns (Mac R3) */
    }
    set(s->balance[0], MUIA_ShowMe, s->lay.balance_sidebar_main ? TRUE : FALSE);
    set(s->balance[1], MUIA_ShowMe, s->lay.balance_main_detail ? TRUE : FALSE);
}

/* I671/I949b: the layout for the full screen the window asks for (D-M4-3), from the screen it opens on (the own SELECTA
 * screen, else the default public screen) and the open ladder step (map/mui_screen.c). Returns 0 when the ladder has no
 * step left. A user-chosen interlaced screen is laid out with the 256-row metrics. */
static uint32_t mui_layout_for_screen(mui_state* s, uint32_t step) {
    uint32_t w, h;
    int laced;
    mui_open_step_t st;
    mui_screen_size(&w, &h, &laced);
    if (!mui_open_ladder(step, w, h, laced, &st)) {
        return 0;
    }
    s->open_layout_w = st.layout_w;
    mui_layout_map_wide(&s->lay, st.layout_w, st.layout_h, g_screen_rtg ? SEL_UIM_TIER_DEFAULT : SEL_UIM_TIER_BITMAP,
                        s->lay.detail_hidden, s->lay.side_wide, s->lay.mode);
    mui_apply_layout(s);
    return 1;
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
    w = mui_open_layout_width(s->open_layout_w, (uint32_t)w); /* I973h: never a layout larger than the ladder step that fit */
    if (w == 0 || (w == s->lay.width && h == s->lay.height)) {
        return;
    }
    if (mui_layout_relayout(&s->lay, (uint32_t)w, (uint32_t)h, g_screen_rtg ? SEL_UIM_TIER_DEFAULT : SEL_UIM_TIER_BITMAP, &flips)) {
        mui_apply_layout(s);
    }
    if (s->side_dyn != NULL && mui_sidebar_chips_single(!g_screen_rtg, s->lay.weight_w[MUI_SLOT_SIDEBAR], g_pixel_aspect_q8) != s->side_single) {
        mui_sidebar_refresh(s); /* I669: the chips now fit, or no longer fit, the sidebar width */
    }
}

/* MUI-P1-3: muimaster.library v19 (MUI 3.8), one Application with one Window. The sentinel counts after the window
 * is open (MUI-P1-5), so ui_open_mui=1 proves the window really opened. */
static void mui_timer_open(mui_state* s);
static void mui_timer_close(mui_state* s);

static void* mui_open(const sel_ui_open_args* a) {
    mui_state* s = &g_state;
    ULONG open = 0;
    uint32_t step;
    Object* strip;
    Object* np_detail;
    Object* np_page;
    int np_compact;
    mui_window_attrs_t wattr;
    s->engine = a != NULL ? a->engine : NULL;
    mui_route_init(&s->route);
    s->find_plugin = a != NULL ? a->find_plugin : NULL;
    g_trace_host = a != NULL ? a->host : NULL;
    g_trace_fn = a != NULL && a->host != NULL ? a->trace : NULL;
    mui_trace("ui: mui_open begin");
    {
        char knob[8];
        s->dbl_race = GetVar((CONST_STRPTR)"Selecta/MUIDblRace", (STRPTR)knob, sizeof knob, 0) >= 0 && mui_list_dbl_race_on(knob);
    }
    MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, 19);
    if (MUIMasterBase == NULL) {
        mui_report(MUI_FAIL_NO_LIBRARY, "");
        return NULL;
    }
    GfxBase = (struct GfxBase*)OpenLibrary((CONST_STRPTR) "graphics.library", 37);
    if (GfxBase == NULL) {
        mui_libs_close();
        mui_report(MUI_FAIL_NO_LIBRARY, "(graphics.library)");
        return NULL;
    }
    mui_trace("ui: muimaster and graphics libraries open");
    mui_theme_init(a);
    mui_trace("ui: theme and screen setup done");
    {   /* I684: the viz plugins the desktop lists, the initial one from the same ui.desktop.viz pref */
        const sel_host* vh = s->engine != NULL ? sel_engine_plugin_host(s->engine) : NULL;
        const char* pref = vh != NULL && vh->config_get != NULL ? vh->config_get(vh, "ui.desktop.viz", "") : "";
        (void)mui_vizset_load(&s->vizset, s->find_plugin, pref);
    }
    strip = mui_menu_strip();
    mui_trace("ui: viz set and menu strip built");
    sel_uim_init(&s->uim);
    /* I675 (D-M4-3): borderless backdrop window on the own screen, normal borders on Workbench (g_scr == NULL). The borderless
     * window has no title: Intuition draws a title bar on any titled window, borderless or not. */
    mui_window_attrs(g_scr != NULL, &wattr);
    /* MUI-P4-17: MUIA_Application_Base is the settings identity only. The shell opened the one ARexx port SELECTA before this
     * UI (arexx_open, apps/selecta/main.c), so MUI takes SELECTA.1 for its own built-in commands and no MUIA_Application_Commands
     * table is given: PLAY and every other command reach the engine through the shell's port, one command set (plan section 5). */
    /* I960c: the detail group is built once; the compact class puts it in the main page group as the Now Playing page, the full class in the detail slot. */
    np_compact = mui_screen_compact();
    np_detail = mui_detail_group(s, np_compact);
    np_page = np_compact ? np_detail : NULL;
    s->app = ApplicationObject, MUIA_Application_Title, (ULONG) "Selecta", MUIA_Application_Base, (ULONG) "SELECTA",
        MUIA_Application_Version, (ULONG) "$VER: Selecta MUI 1.0",
        SubWindow, s->win = WindowObject, TH_SCR MUIA_Window_Title, wattr.titled ? (ULONG) "Selecta" : (ULONG)NULL, MUIA_Window_ID, SEL_MAKE_ID('S', 'E', 'L', 'M'), MUIA_Window_AppWindow, TRUE,
            MUIA_Window_Borderless, (ULONG)wattr.borderless, MUIA_Window_Backdrop, (ULONG)wattr.backdrop, MUIA_Window_DragBar, (ULONG)wattr.dragbar,
            MUIA_Window_DepthGadget, (ULONG)wattr.depthgadget, MUIA_Window_SizeGadget, (ULONG)wattr.sizegadget,
            MUIA_Window_CloseGadget, (ULONG)wattr.closegadget,
            /* D-M4-3: full screen by default; width and height are 100 percent of the public screen area MUI can use.
             * MUI never sizes a window beyond its contents' maximum height, so every body column and every page of a
             * page group must have an unbounded maximum (a lone Text object is bounded; I1307 left 150 rows empty). */
            MUIA_Window_Width, MUIV_Window_Width_Screen(100), MUIA_Window_Height, MUIV_Window_Height_Screen(100),
            MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered, MUIA_Window_Menustrip, (ULONG)strip,
            WindowContents, s->root = VGroup, TH_FONT TH_TAGS(MUI_TH_WINDOW, MUI_FRAME_NONE)
                Child, s->slot[MUI_SLOT_TOPBAR] = mui_topbar_group(s),
                Child, s->slot[MUI_SLOT_SUGGESTION] = mui_aatext("Window is narrow: try the mini layout", MUI_AA_TONE_NORMAL, MUI_AA_ALIGN_LEFT, 1),
                Child, HGroup,
                    Child, s->slot[MUI_SLOT_SIDEBAR] = mui_sidebar_group(s, g_screen_rtg),
                    Child, s->balance[0] = UP_NewBalance(NULL),
                    Child, s->slot[MUI_SLOT_MAIN] = mui_main_pages(s, np_page),
                    Child, s->balance[1] = UP_NewBalance(NULL),
                    Child, s->slot[MUI_SLOT_DETAIL] = np_compact ? (Object*)MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE) : np_detail,
                End,
                Child, s->slot[MUI_SLOT_PLAYER] = mui_player_group(s),
                Child, s->keys = mui_keys_new((ULONG)CTL_KEY_BASE, (ULONG)CTL_DOOR_BASE, (ULONG)CTL_PANE_BASE), /* I1165, I1171 */
            End, End, End;
    if (s->app == NULL) {
        if (strip != NULL) {
            MUI_DisposeObject(strip);
        }
        mui_screen_close();
        mui_libs_close();
        mui_report(MUI_FAIL_NO_WINDOW, "(MUI could not create the objects)");
        return NULL;
    }
    mui_keys_targets(s->str_search, s->list); /* I1165: Down from Search enters the results */
    mui_trace("ui: application and window object tree built (classes created)");
    fprintf(stderr, "selecta mui: tier font on %d window roots (%s)\n", g_font_applied, g_scr_font != NULL ? "applied" : "none");
    DoMethod(s->win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, s->app, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);
    g_drop_state = s;
    g_drop_hook.h_Entry = (__typeof__(g_drop_hook.h_Entry))(void (*)(void))HookEntry;
    g_drop_hook.h_SubEntry = (__typeof__(g_drop_hook.h_SubEntry))(void (*)(void))mui_drop_hook;
    DoMethod(s->win, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime, s->app, 3, MUIM_CallHook, (ULONG)&g_drop_hook, MUIV_TriggerValue);
    /* I671: pick the breakpoint from the screen the window opens on BEFORE the open. The full three-column layout's
     * minimum height (seven 24 px sidebar doors plus the player rows) exceeds a 640x256 AGA screen, and MUI 3.8 refuses
     * a window whose minimum does not fit (MUIA_Window_Open stays 0). Applying the 1280x720 layout only after the open
     * never got that far. */
    /* I949b open ladder: the same window object is opened again after each refusal with the next more compact layout
     * (rig 2 proved MUI 3.8 reopens after ShowMe changes); the failure is reported only after the last step is refused. */
    aa_trace_set(mui_trace); /* I625a6: the package's method-level lines land in the same --log */
    for (step = 0; mui_layout_for_screen(s, step); step++) {
        {
            char tb[64];
            snprintf(tb, sizeof tb, "ui: ladder step %lu layout applied, MUIA_Window_Open set begins", (unsigned long)step);
            mui_trace(tb);
        }
        set(s->win, MUIA_Window_Open, TRUE);
        GetAttr(MUIA_Window_Open, s->win, &open); /* MUI opens the window inside the set; 0 here means it failed */
        mui_trace(open ? "ui: window open attempt returned, window open (ladder step)" : "ui: window open attempt returned, refused (ladder step)");
        {
            /* I671 measurement: the minimum the root group asked for (MUIM_AskMinMax ran inside the open attempt, the values
             * stay in the object after a refusal) next to the screen it had to fit. --log on the rig records one line per step. */
            struct Screen* scr = g_scr != NULL ? g_scr : LockPubScreen(NULL);
            /* I963: the maximum heights next to it; a body panel whose maximum is bounded caps the window height. */
            {   /* I625a6: what the step refused on, in the --log as well as on stderr */
                char tb[160];
                snprintf(tb, sizeof tb, "ui: ladder step %lu %s: window minimum %ldx%ld, screen %dx%d", (unsigned long)step,
                         open ? "opened" : "refused", (long)_minwidth(s->root), (long)_minheight(s->root),
                         scr != NULL ? (int)scr->Width : 0, scr != NULL ? (int)scr->Height : 0);
                mui_trace(tb);
            }
            fprintf(stderr, "selecta mui: ladder step %lu window minimum %ldx%ld max height %ld (sidebar %ld main %ld detail %ld), screen %dx%d, open %lu\n",
                    (unsigned long)step, (long)_minwidth(s->root), (long)_minheight(s->root), (long)_maxheight(s->root),
                    (long)_maxheight(s->slot[MUI_SLOT_SIDEBAR]), (long)_maxheight(s->slot[MUI_SLOT_MAIN]), (long)_maxheight(s->slot[MUI_SLOT_DETAIL]),
                    scr != NULL ? (int)scr->Width : 0, scr != NULL ? (int)scr->Height : 0, (unsigned long)open);
            {   /* I1092c: the minimum height of every slot at this step (a hidden slot must not count) */
                uint32_t sl;
                for (sl = 0; sl < MUI_SLOT_COUNT; sl++) {
                    fprintf(stderr, "selecta mui: ladder step %lu slot %lu show %lu min %ldx%ld\n", (unsigned long)step, (unsigned long)sl,
                            (unsigned long)s->lay.show[sl], (long)_minwidth(s->slot[sl]), (long)_minheight(s->slot[sl]));
                }
            }
            if (s->main_pages != NULL) {
                /* I963 (Johan: sidebar clicks redraw slowly at 640x256): each main page's min/max height next to the slot height, so
                 * a page whose minimum exceeds the slot (MUI relayouts on each switch) shows in the --log. */
                struct List* kids = NULL;
                Object* state;
                Object* kid;
                unsigned long pg = 0;
                get(s->main_pages, MUIA_Group_ChildList, &kids);
                state = kids != NULL ? (Object*)kids->lh_Head : NULL;
                while (state != NULL && (kid = NextObject(&state)) != NULL) {
                    fprintf(stderr, "selecta mui: ladder step %lu main page %lu min height %ld max height %ld, slot height %ld\n",
                            (unsigned long)step, pg++, (long)_minheight(kid), (long)_maxheight(kid),
                            open ? (long)_mheight(s->main_pages) : -1L);
                }
            }
            if (scr != NULL && scr != g_scr) {
                UnlockPubScreen(NULL, scr);
            }
        }
        if (open) {
            break;
        }
    }
    if (!open) {
        static char why[96];
        snprintf(why, sizeof why, "(the layout needs %ldx%ld px, more than the screen shows)", (long)_minwidth(s->root),
                 (long)_minheight(s->root));
        MUI_DisposeObject(s->app);
        s->app = NULL;
        s->win = NULL;
        mui_screen_close();
        mui_libs_close();
        mui_report(MUI_FAIL_NO_WINDOW, why);
        return NULL;
    }
    s->wake_mask = 0;
    mui_timer_open(s);
    s->lay.width = 0; /* mui_layout_for_screen applied the screen-size layout; the first resize check moves it to the real size */
    s->lay.breakpoint = SEL_UIM_BP_COUNT; /* no breakpoint yet: the first real size always applies */
    mui_trace("ui: window open done, timer open");
    mui_check_resize(s);
    mui_trace("ui: first resize check and layout done");
    mui_refresh(s);
    mui_trace("ui: first refresh (data into the lists, first draw) done");
    s->opens++;
    return s;
}

/* I724: the I716 race made deterministic. A list refresh between the click and its handling resets UPA_List_Active to the engine cursor
 * row (mui_refresh: SetAttrs UPA_List_Active). With the knob on, do exactly that SetAttrs before the handler runs, to the first window row
 * (".." in the rig step). A handler that reads UPA_List_Active then acts on that row, one that reads the picked row does not.
 * Not done through VIEW_CURSOR + mui_refresh: sel_engine_send_line only queues the line, the engine applies it in its next pump
 * (host loop), so the refresh in the same call still saw the old cursor and left UPA_List_Active on the clicked row (round 7 probe:
 * UPA_List_Active 6 before and after, picked row 6). The click-pending guard is disarmed as the real race happens after it caught up. */
static void mui_dbl_race_inject(mui_state* s) {
    mui_list_pending_clear(&s->list_pending);
    s->list_active_set = 0;
    SetAttrs(s->list, UPA_List_Active, (ULONG)0, TAG_DONE);
    s->p4[P4_DBLRACE]++;
}

static int mui_io_new_input(void* ctx, uint32_t* sigs) {
    ULONG got = 0;
    LONG id = (LONG)DoMethod(((mui_state*)ctx)->app, MUIM_Application_NewInput, &got);
    *sigs = (uint32_t)got;
    mui_check_resize((mui_state*)ctx);
    if (id >= MENU_ID_BASE && id < MENU_ID_BASE + (LONG)g_menu_n) {
        mui_menu_fire((mui_state*)ctx, (uint32_t)(id - MENU_ID_BASE));
    } else if (id >= CTL_BTN_BASE) {
        if (id == CTL_LIST_DBL && ((mui_state*)ctx)->dbl_race) {
            mui_dbl_race_inject((mui_state*)ctx);
        }
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
    if (s->col_brf.inflight) { /* the engine still writes the answer into the buffer until it is taken */
        uint32_t done = 0;
        int ok = 0;
        while (s->engine != NULL && !sel_engine_song_briefs_take(s->engine, s->col_brf.buf, s->col_brf.cap, &done, &ok)) {
            Delay(1);
        }
    }
    free(s->col_brf.buf);
    s->col_brf.buf = NULL;
    if (s->app != NULL) {
        MUI_DisposeObject(s->app); /* closes the window and frees the children */
        s->app = NULL;
        s->win = NULL;
        s->canvas = NULL;
        mui_canvas_class_free();
        mui_vizarea_class_free();
        mui_aawrap_free(); /* I685f: the list hooks, after the objects are gone */
        mui_aatext_class_free();
        mui_aapanel_class_free();
        mui_aatabs_class_free();
        mui_aabalance_class_free();
        mui_aaslider_class_free();
        mui_aacycle_class_free();
        mui_aalist_class_free(); /* I667i: also frees the AAScroller class */
        mui_aafield_class_free();
        mui_aabtn_class_free();
        mui_listview_class_free();
        mui_keys_class_free();
    }
    mui_timer_close(s);
    if (MUIMasterBase != NULL) {
        mui_screen_close();
        mui_libs_close();
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
    {
        char l1[sizeof text]; /* I1139 */
        EasyRequest(NULL, &es, NULL, (ULONG)mui_l1(l1, sizeof l1, text));
    }
}
#else
/* Host build: no window, so a layout change has nothing to apply (I1173). */
static void mui_on_event(void* state, const sel_message* evt) {
    (void)state;
    (void)evt;
}

static void mui_report(mui_fail why, const char* detail) {
    fprintf(stderr, "%s %s\n", mui_fail_text(why), detail);
}

static void* mui_open(const sel_ui_open_args* a) {
    (void)a;
    mui_report(mui_present() ? MUI_FAIL_NO_WINDOW : MUI_FAIL_NO_LIBRARY, "");
    return NULL; /* the host has no MUI: the shell keeps its current UI */
}

/* Host: no screen, no AA classes; the live THEME runs the same apply path with nothing to repaint (the contract test counts it). */
static void mui_live_none(void* ctx, const uint32_t* tokens) {
    (void)ctx;
    (void)tokens;
}
static void mui_live_none_classes(void* ctx) {
    (void)ctx;
}
static int mui_theme_live_now(void) {
    mui_theme_live_ops ops;
    ops.palette = mui_live_none;
    ops.classes = mui_live_none_classes;
    ops.ctx = NULL;
    return mui_theme_apply_live(&ops, MUI_THEME_ENV_PATH, g_theme_arg[0] != '\0' ? g_theme_arg : NULL);
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
    put_tag(&g_hooks[7], SEL_UI_THEME, SEL_TAG_FN(mui_theme_hook));
    put_tag(&g_hooks[8], SEL_UI_ON_EVENT, SEL_TAG_FN(mui_on_event));
    put_tag(&g_hooks[9], SEL_TAG_END, 0);

    put_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_UI);
    put_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "mui");
    put_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    put_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    put_tag(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
