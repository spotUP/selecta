/* MUI plugin mapping functions (plan 2026-10-25-amiga-mui.md section 2.2). Pure C: no <proto/muimaster.h>, so the
 * host builds and tests this directory (MUI-P1-1). */
#ifndef SEL_MUI_MAP_H
#define SEL_MUI_MAP_H

#include <stddef.h>
#include <stdint.h>

#include <selecta/engine.h>
#include <selecta/uimodel.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel/palette.h>
#include <selecta/uimodel/libfilter.h>
#include <selecta/uimodel/panels.h>
#include <selecta/uimodel/sources.h>

/* The view styles the MUI UI offers. An RTG screen offers every style (6). A planar screen shows artwork and grid tiles
 * as cached 1-bit dithered images (MUI-P5-7, I509), so all but cover flow (5); the classic three alone no longer fit. */
uint32_t mui_style_mask(int screen_is_rtg);

/* MUI-P1-7: the SEL_UI_WAIT contract as a loop over two callbacks, so the host tests it and the Amiga build runs the
 * same code. new_input is MUIM_Application_NewInput (returns the ReturnID, MUI_WAIT_ID_QUIT for the quit id, and the
 * signals MUI wants waited on); wait blocks up to timeout_ms on a signal mask and returns the signals that arrived. */
#define MUI_WAIT_ID_QUIT (-1)
typedef struct mui_wait_io {
    int (*new_input)(void* ctx, uint32_t* sigs);
    uint32_t (*wait)(void* ctx, uint32_t mask, uint32_t timeout_ms);
    uint32_t break_mask; /* Ctrl-C: ends the UI like the close gadget */
    uint32_t wake_mask;  /* the engine wake signal: part of every wait mask, counts as handled */
} mui_wait_io;
/* 0 timeout, 1 input handled, -1 quit. */
int mui_wait_step(const mui_wait_io* io, void* ctx, uint32_t timeout_ms);

/* MUI-P1-6: is MUI 3.8 (muimaster.library v19) usable. The probe is the library open on the Amiga and "absent" on the
 * host until a test installs a fake. The plugin entry returns no descriptor when it says no, so the registry never
 * lists `mui` and `--ui mui` is refused as unregistered. */
typedef int (*mui_probe_fn)(void);
void mui_set_probe(mui_probe_fn probe);
int mui_present(void);

/* MUI-P1-4: the distinct failure texts printed and shown in the EasyRequest when the UI cannot open. */
typedef enum mui_fail { MUI_FAIL_NO_LIBRARY = 0, MUI_FAIL_NO_WINDOW = 1 } mui_fail;
const char* mui_fail_text(mui_fail why);

/* MUI-P2-3: one Menustrip row per action-table row with a non-empty menu path. "Menu/Item" gives title and item,
 * "Menu/Sub/Item" a sub menu between them. The shortcut is the key when it is one character (MUI shortcuts are single
 * keys); longer key names (Enter, M-n) are shown as `key` text only. Returns the row count even above cap. */
typedef struct mui_menu_item {
    char title[32];
    char sub[32];
    char item[48];
    const char* key;
    char shortcut;
    const char* id;
} mui_menu_item;
uint32_t mui_menu_build(mui_menu_item* out, uint32_t cap);
/* MUI-P3-8: as mui_menu_build, but a View > Style row is listed only when its style is in style_mask (mui_style_mask). */
uint32_t mui_menu_build_for(mui_menu_item* out, uint32_t cap, uint32_t style_mask);
/* MUI-P2-4: the protocol line a menu item sends: the action table's line for the id, the very line the desktop sends
 * (desktop_ui.c desk_action_by_id). NULL for an unknown id. */
const char* mui_menu_line(const char* id);

/* MUI-P2-1: the five layout regions as fixed object slots. The objects are created once; a breakpoint only changes
 * which are shown (MUIA_ShowMe), never how many exist (MUI-P2-2). weight_w feeds MUIA_HorizWeight of the body row
 * children (sidebar, main, detail) and weight_h MUIA_VertWeight of body versus player; a hidden slot has show 0. */
enum { MUI_SLOT_SUGGESTION = 0, MUI_SLOT_SIDEBAR, MUI_SLOT_MAIN, MUI_SLOT_DETAIL, MUI_SLOT_PLAYER, MUI_SLOT_TOPBAR, MUI_SLOT_COUNT };
typedef struct mui_layout_map_t {
    uint32_t width, height;
    uint32_t breakpoint; /* SEL_UIM_BP_* from sel_uim_breakpoint_for */
    uint32_t tier;       /* SEL_UIM_TIER_* the map was computed for (I615d) */
    uint8_t show[MUI_SLOT_COUNT];
    uint32_t weight_w[MUI_SLOT_COUNT];
    uint32_t weight_h[MUI_SLOT_COUNT];
    uint8_t balance_sidebar_main, balance_main_detail; /* Balance object between two shown panels */
    uint8_t compact;       /* I949c: the compact class (mui_layout_compact): the sidebar is the labelled door rail */
    uint8_t detail_hidden; /* I798: the Now Playing column is hidden by the top bar toggle (Mac I619 detail_hidden) */
} mui_layout_map_t;
/* I949c: the compact class of a 640x256-class screen: the bitmap tier at the icon-sidebar breakpoint (width 600-699) and at most
 * MUI_COMPACT_MAX_ROWS rows. Its sidebar is the labelled door rail uimodel asks for (SIDEBAR_RAIL_LABELLED, 160 px): doors only, no
 * brand row, and the Your Library block lives on the Library page. mui_layout_map sets `compact` from this. */
enum { MUI_COMPACT_MAX_ROWS = 300 };
uint32_t mui_layout_compact(uint32_t width, uint32_t height, uint32_t tier);
/* I949d: the player band of the compact class: ONE band at most MUI_PLAYER_COMPACT_MAX_H px high. Left the art (MUI_PLAYER_ART_PX wide, the
 * height from the pixel aspect so it is square on screen: 28 rows at aspect 512) with title and artist; centre the transport row and the
 * seek row; right the volume and the format/status on ONE row. Album and speed leave the band: they are menu items only (the action
 * table rows next_album .. tempo_down, listed by mui_menu_build), no overflow button. A non-compact class keeps the Mac's three columns. */
enum { MUI_PLAYER_COMPACT_MAX_H = 36, MUI_PLAYER_COMPACT_PAD = 4 };
/* I973 (design 2026-11-02-I973-mui-player-bar-design.md sections 3, 4, 6a): the bar is ONE row in both classes: [subsong] transport
 * clock | seek | length speaker volume, every child on one centre line; art + title/artist only from MUI_PLAYER_LEFT_MIN_W px (the Mac
 * rule). No Stop, no remaining time, no status or format text, no heart, no album/speed extras on the bar (menu only). */
enum { MUI_PLAYER_LEFT_MIN_W = 900, MUI_PLAYER_CLOCK_W = 64 /* "00:00:00" at MUI_FONT_W */, MUI_PLAYER_SUB_LABEL_W = 56, MUI_PLAYER_TITLES_W = 160, MUI_VOLUME_FIX_W = 96, MUI_BAR_TRANSPORT_N = 5 };
typedef struct mui_player_band_t {
    uint8_t compact;      /* 1 = the compact class (640x256 class screen) */
    uint8_t band_n;       /* bands: always 1 */
    uint8_t extras;       /* 0: the album and speed buttons are menu items only, in both classes */
    uint8_t right_rows;   /* 1: the right end is the speaker and the volume on the one row */
    uint32_t art_w, art_h;
    uint32_t height;      /* bar height in rows: max(btn_px, art_h) + 2 x pad; the compact class is capped at MUI_PLAYER_COMPACT_MAX_H */
    uint32_t btn_px;      /* height in rows of every icon-only bar button (max of icon height and text height, plus 2 x pad_y) */
    uint32_t btn_w;       /* width in pixels of the same button: btn_px x aspect / 256, so it is a square on screen (tall pixels: wider in px) */
    uint32_t btn_gap;     /* spacing inside the transport group */
    uint32_t gap;         /* spacing between the bar groups */
    uint32_t n_transport; /* 5: Shuffle, Previous, Play, Next, Repeat (no Stop) */
    uint32_t show_left;   /* 1 = art and title/artist are children of the bar (bar_w >= MUI_PLAYER_LEFT_MIN_W) */
    uint32_t sub_w;       /* width of the subsong group (label + minus + plus); the group is shown only when the song has subsongs */
    uint32_t track_h;     /* seek and volume track height (centred in the slider box) */
    uint32_t slider_h;    /* slider box height in rows */
    uint32_t text_h;      /* clock/length text box height in rows */
} mui_player_band_t;
/* bar_w: the screen (window) width in px; font_px: mui_aa_font_px of the screen. Counts mui_player_bar_builds. */
void mui_player_band(mui_player_band_t* out, uint32_t compact, int aspect_q8, uint32_t bar_w, uint32_t font_px);
/* Vertical centre (rows from the bar top) of a child of height child_h in the bar: every child of the bar returns the same value +-1. */
uint32_t mui_player_centre_y(const mui_player_band_t* pb, uint32_t child_h);
/* Minimum bar width in px: transport + clock + MUI_SLIDER_MIN + length + speaker + volume + gaps (without the subsong group and the art block). */
uint32_t mui_player_min_width(const mui_player_band_t* pb);
/* Sentinel: bars built (mui_player_band calls) since start. */
uint32_t mui_player_bar_builds(void);

/* I973b: an icon button's size request, pure. fixed_w / fixed_h in px (0 = none). The deltas are added to the Area's own insets.
 * A fixed side has min = def = max, so no weight can stretch the square; a labelled button keeps the unbounded width. */
typedef struct mui_btn_box {
    uint32_t min_w, def_w, min_h, def_h;
    uint32_t w_fixed; /* 1: MaxWidth = MinWidth, else unbounded */
    uint32_t h_fixed; /* always 1 today: a button never grows taller than its contents */
} mui_btn_box_t;
void mui_aabtn_minmax(uint32_t content_w, uint32_t content_h, uint32_t fixed_w, uint32_t fixed_h, uint32_t toggle, mui_btn_box_t* out);
/* I949e2: height of an art frame width px wide so it is square on screen: width x 256 / aspect (mui_aa_rows). The player art is
 * MUI_PLAYER_ART_PX wide, the detail art MUI_DETAIL_ART_PX. 256 (square pixels) gives the width. */
#define MUI_DETAIL_ART_PX 64
uint32_t mui_art_height(uint32_t width, int aspect_q8);
/* I949e2: the vizarea's vertical sizes in rows (the width minimum is pixels): minimum, default and the cap of a non-tall area, each a
 * square-pixel length counted in rows of a screen of aspect_q8. */
typedef struct mui_vizarea_h_t {
    int min_h, def_h, cap_h;
} mui_vizarea_h_t;
void mui_vizarea_heights(int aspect_q8, mui_vizarea_h_t* out);
/* Sentinel: compact bands built since start. */
uint32_t mui_player_band_compact_builds(void);
/* Layout rows of a screen: half of an interlaced screen's rows (a user-chosen laced screen uses the 256-row metrics, I949b). */
uint32_t mui_layout_rows(uint32_t screen_h, int laced);
/* I615d: `tier` is SEL_UIM_TIER_DEFAULT on an RTG screen (top bar slot shown), SEL_UIM_TIER_BITMAP on a native planar screen. */
void mui_layout_map(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier);
/* I798 (T2, R3): as mui_layout_map with the Now Playing column hidden when `detail_hidden`; relayout keeps the flag. */
void mui_layout_map_panel(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t detail_hidden);
/* I798: 1 when the Hide / Show Now Playing button exists (three column breakpoint, top bar shown), as the Mac np_toggle_rect. */
uint32_t mui_np_toggle_shown(const mui_layout_map_t* m);
/* Recompute for a new window size. Returns 1 when the breakpoint changed (the caller re-applies ShowMe, *flips counts
 * the slots whose visibility changed), 0 when only weights move. */
uint32_t mui_layout_relayout(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t* flips);

/* MUI-P2-5: the transport row. Six buttons; the action each sends depends on the frame (Play/Pause on the play state,
 * Repeat cycles off, all, one, Random toggles). Labels and ids come from the action table, so the text is the full
 * English words of the desktop UI. Seek is a Slider 0..MUI_SEEK_MAX bound to progress_q16, volume 0..MUI_VOLUME_MAX. */
#define MUI_TRANSPORT_ROW1 6u /* I667b5: Previous, Play, Next, Repeat and Random are icon-only, so all six buttons share the first row with the volume (row 1 is empty) */
enum { MUI_BTN_PREV = 0, MUI_BTN_PLAY, MUI_BTN_NEXT, MUI_BTN_STOP, MUI_BTN_REPEAT, MUI_BTN_RANDOM, MUI_BTN_COUNT };
#define MUI_SEEK_MAX 65536
#define MUI_VOLUME_MAX 100
/* Action id the button sends for this frame; NULL for an unknown button. */
const char* mui_transport_action(uint32_t button, const sel_uim_frame* f);
/* Button label: the action's label for this frame. */
const char* mui_transport_label(uint32_t button, const sel_uim_frame* f);
/* I667b: longest label over every state of a transport button (sizes the AAButton). */
const char* mui_transport_widest(uint32_t button);
/* I667b5 (owner 2026-10-29): 1 when the transport button shows only its icon (all six, Stop too); the label is the tooltip. */
int mui_transport_icon_only(uint32_t button);
/* I796 (drift B3): the transport buttons left to right as the Mac player bar draws them (draw_transport: Shuffle, Previous, Play, Next,
 * Repeat); Stop has no Mac button and follows Repeat. Position k in 0..MUI_BTN_COUNT-1 gives the MUI_BTN_* index. */
uint32_t mui_player_transport_at(uint32_t k);
/* I796 (drift B1, B2): the player bar's three columns. Weights follow the Mac's 300 px player-left of a 900 px bar; the art frame is the
 * Mac's 56 px square. */
#define MUI_PLAYER_W_LEFT 33
#define MUI_PLAYER_W_CENTRE 47
#define MUI_PLAYER_W_RIGHT 20
#define MUI_PLAYER_ART_PX 56
/* 1 when the button is lit: Repeat while repeating (all or one), Shuffle while shuffle is on; drawn as the SELECTED face because the icon
 * does not change with the state. */
int mui_transport_on(uint32_t button, const sel_uim_frame* f);

/* The lines the sliders send, the very lines the desktop bars send (desktop_ui.c: SEEK <ms>, VOLUME <0..100>). The seek
 * line needs the song length: returns 0 (no line) when duration_ms is 0, else 1. Values clamp to the slider ranges. */
uint32_t mui_seek_line(uint32_t q16, uint32_t duration_ms, char* out, size_t cap);
void mui_volume_line(int32_t level, char* out, size_t cap);

/* MUI-P2-6: frame fields to object attributes. Each of the ten attributes belongs to exactly one dirty region, so a
 * frame update calls set() only for the attributes whose region bit is in the mask. SEL_UIM_DIRTY_QUEUE has no
 * attribute here (the queue list is MUI-P3/P4). */
enum {
    MUI_ATTR_TITLE = 0, MUI_ATTR_ARTIST, MUI_ATTR_FORMAT, MUI_ATTR_LENGTH, MUI_ATTR_CLOCK, MUI_ATTR_REMAINING,
    MUI_ATTR_SEEK, MUI_ATTR_STATUS, MUI_ATTR_VOLUME, MUI_ATTR_PLAYSTATE, MUI_ATTR_COUNT
};
uint32_t mui_attr_region(uint32_t attr);              /* the one SEL_UIM_DIRTY_* bit, 0 for an unknown attr */
uint32_t mui_dirty_attrs(uint32_t dirty);             /* bit mask of (1u << MUI_ATTR_*) to refresh */
const char* mui_attr_text(uint32_t attr, const sel_uim_frame* f); /* text attrs; NULL for slider and button attrs */
/* I667f3: the sample string a time label is sized to, so the seek row gives the labels their content width and the seek slider the
 * rest. NULL for an attribute that keeps the flexible default (title, artist, format, status). */
const char* mui_attr_fixwidth(uint32_t attr);

/* MUI-P2-7: every action key as a binding. Keys of one character are Right Amiga menu shortcuts (the Menustrip
 * shortcut); named and modified keys (C-Enter, M-n, Space) go through a window event handler as code plus modifier
 * bits of uimodel/keys.h. Mode keys: Right Amiga + 1..5 and + I switch ui.mui.mode (M4 plan 3.4, Q7). */
typedef struct mui_key_bind {
    const char* id;
    uint32_t code, mods;
    uint32_t shortcut; /* 1: Right Amiga menu shortcut, 0: event handler */
} mui_key_bind;
uint32_t mui_keys_build(mui_key_bind* out, uint32_t cap); /* returns the row count even above cap */
typedef struct mui_mode_key {
    char key;
    const char* mode;
    const char* line; /* PREF_SET ui.mui.mode <mode> */
} mui_mode_key;
const mui_mode_key* mui_mode_keys(uint32_t* count);

/* MUI-P3-1..4, P3-7: the list models. A back end is chosen once at open from the MUI-P0-1 facts: `nlist` (NList
 * columns and sort, Listtree for the tree style) when NList is installed, `plain` (3.8 List with a Format string,
 * indented rows) otherwise. Both consume the same sel_view_state window and give the same labels, cursor and columns. */
typedef struct mui_list_backend {
    const char* name;  /* "nlist" or "plain" */
    uint32_t sortable; /* NList sort */
    uint32_t native_tree; /* Listtree draws the depth, so the row text carries no indent */
} mui_list_backend;
const mui_list_backend* mui_list_backend_pick(int nlist_present, int listtree_present);

#define MUI_LIST_TEXT 160
typedef struct mui_list_entry {
    uint32_t index;    /* index in the whole list (window_start + position) */
    uint32_t kind;     /* sel_row_kind */
    uint32_t depth;
    uint32_t node;     /* 1 for a SEL_ROW_NODE row */
    uint32_t expanded;
    char label[MUI_LIST_TEXT]; /* row text without indent or marker */
    char text[MUI_LIST_TEXT];  /* what the List shows: indent, +/- marker for nodes, count */
} mui_list_entry;
/* Fill out[] from the visible window only (at most window_rows and cap entries); *active is the List cursor
 * position inside the window, -1 when the cursor row is outside it. Returns the entries filled. */
uint32_t mui_list_build(const mui_list_backend* be, mui_list_entry* out, uint32_t cap, const sel_view_state* v, int32_t* active);
/* MUI-P3-1: the user's row click is sent to the engine as VIEW_CURSOR, but sel_engine_state() is only a snapshot the pump
 * updates later. Until it shows the new cursor, a list rebuild must keep the clicked row selected, not the snapshot's old
 * cursor row (else a double-click right after reads the old row back and activates it). */
typedef struct mui_list_pending {
    int armed;
    int32_t target; /* the row the user selected (window position) */
    int32_t base;   /* the snapshot's cursor position when it was sent */
} mui_list_pending;
void mui_list_pending_arm(mui_list_pending* p, int32_t target, int32_t snapshot_active);
void mui_list_pending_clear(mui_list_pending* p);
/* The row a rebuild selects: the pending target while the snapshot still shows the cursor it had at arm time; the
 * snapshot's own position once it moved (the pending selection is then settled and cleared). */
int32_t mui_list_pending_resolve(mui_list_pending* p, int32_t snapshot_active);
/* MUI-P3-1: the row a double click acts on. The list reports the row the user clicked (picked, -1 when none); the
 * List cursor (active) can be moved by a list refresh between the click and the app loop handling it, so the clicked
 * row wins and the cursor is only the fallback. */
int32_t mui_list_dbl_row(int32_t picked, int32_t active);
/* I724: ENV:Selecta/MUIDblRace value (read once at open) -> knob on: any text but empty or "0". */
int mui_list_dbl_race_on(const char* value);
extern uint32_t mui_list_dbl_row_calls; /* sentinel: how often the decision ran */
/* Tree style: the engine line a click on a node row sends (VIEW_EXPAND/VIEW_COLLAPSE <node id>); "" for a non-node. */
void mui_list_toggle_line(const sel_view_row* r, char* out, size_t cap);
/* Drilldown breadcrumb "Queue by folder  Drilldown > a > b" from the shared title function. */
void mui_list_breadcrumb(const sel_view_state* v, char* out, size_t cap);

enum { MUI_COL_TITLE = 0, MUI_COL_ARTIST, MUI_COL_ALBUM, MUI_COL_LENGTH, MUI_COL_FORMAT, MUI_COL_COUNT };
/* Visible columns in display order for a SEL_UIM_BP_*; hidden by width: format first, then album, then length. */
uint32_t mui_columns_for(uint32_t breakpoint, uint8_t out[MUI_COL_COUNT]);
/* MUIA_List_Format string for those columns: "BAR," per column but the last. */
void mui_columns_format(uint32_t breakpoint, char* out, size_t cap);
/* I667i2: the weight of each shown column (0 = fixed to its sample width), the table the format string uses; returns the column count. */
/* I667i2: the lyrics as rows of an AAList: number of newline separated lines (a trailing newline adds none, empty text has 0) and line row
 * copied into out (cut to cap - 1; empty past the last line). */
uint32_t mui_text_lines(const char* text);
void mui_text_line(const char* text, uint32_t row, char* out, size_t cap);
uint32_t mui_columns_weights(uint32_t breakpoint, int weight_out[MUI_COL_COUNT]);
/* Cell text of one row; "" for the columns the engine row does not carry (the view model rows hold title and artist). */
const char* mui_row_cell(const sel_view_row* r, uint32_t column);
/* MUI-P3-4: the album, length and format cells of one row. The engine rows carry only title and artist, so these come from the
 * snapshot: the format is the file extension of the row's song (queue rows: its queue entry's path, else the row text); the
 * length is the open song's duration on its own queue row, else the shared unknown clock "--:--" (the engine keeps no length per
 * queued song); the album is what the caller got from sel_engine_song_group for the row's path ("" when uncatalogued). */
typedef struct mui_row_extra {
    char album[64];
    char length[16];
    char format[40]; /* the I819 format label, SEL_FORMAT_LABEL_MAX */
} mui_row_extra;
void mui_row_extra_fill(const sel_state* st, const sel_view_row* r, const char* album, mui_row_extra* out);

/* MUI-P4-1: Home rows. SEL_ROW_HOME rows carry `key` plus `album` (song to play versus album to open) and `composer`. */
typedef enum mui_home_kind { MUI_HOME_NONE = 0, MUI_HOME_SONG, MUI_HOME_ALBUM, MUI_HOME_COMPOSER } mui_home_kind;
mui_home_kind mui_home_kind_of(const sel_view_row* r);
/* The three engine lines a click on window row `row` sends (cursor, activate, ask the home source again, as the
 * desktop shelf tile does); returns 3, or 0 for a row that is not a Home entry. */
uint32_t mui_home_lines(const sel_view_state* v, uint32_t row, char lines[3][48]);

/* I795 (drift doc P1): the Home page model. The quick-pick rows are the SEL_ROW_HOME rows after the quick heading (at most six), each
 * shelf is a heading plus the SEL_ROW_HOME rows after it. `shelf_id` counts every shelf heading (1-based, the id Show all routes with),
 * also those past the MUI_HOME_SHELF_MAX slots. */
#define MUI_HOME_QUICK_MAX 6u
#define MUI_HOME_SHELF_MAX 4u
#define MUI_HOME_SHELF_ROWS 5u /* rows of a shelf the Home page lists; Show all has the rest */
typedef struct mui_shelf {
    uint32_t heading, first, count, id; /* window row of the heading, of its first entry, entries under it, 1-based shelf id */
} mui_shelf;
typedef struct mui_home_page {
    uint32_t quick_first, quick_n;
    mui_shelf shelf[MUI_HOME_SHELF_MAX];
    uint32_t shelf_n, shelf_id;
} mui_home_page;
void mui_home_split(const sel_view_state* v, mui_home_page* out);
/* The shelf with 1-based id `id` (any shelf, past the slots too), all its entries counted. 1 when found. */
uint32_t mui_shelf_find(const sel_view_state* v, uint32_t id, mui_shelf* out);

/* I795 (P4): the Browse tiles, from the FORMATS_SHOW and COMPOSERS_SHOW answers in the engine state, then one Sources tile. */
#define MUI_BROWSE_FORMATS 8u
#define MUI_BROWSE_COMPOSERS 8u
#define MUI_TILE_MAX (MUI_BROWSE_FORMATS + MUI_BROWSE_COMPOSERS + 1u)
enum { MUI_TILE_FORMAT = 0, MUI_TILE_COMPOSER = 1, MUI_TILE_SOURCES = 2 };
typedef struct mui_btile {
    uint32_t kind, count;
    char label[48], arg[96];
} mui_btile;
uint32_t mui_browse_tiles(const sel_state* st, mui_btile* out, uint32_t cap);
void mui_tile_caption(const mui_btile* t, char* out, size_t cap);

/* I795 (P5): the engine line a Search facet chip sends: VIEW_CURSOR on the heading of that group of results (facet 0, All: the top).
 * Returns 1 when `out` holds a line, 0 when the results have no such group. The names are sel_uim_search_facets. */
uint32_t mui_search_facet_line(const sel_view_state* v, uint32_t facet, char* out, size_t cap);

/* MUI-P4-0: the sidebar doors, built from the shared uimodel door table (include/selecta/uimodel/doors.h). The label is the
 * table's; the line is the engine line that switches the main view to the door's page, NULL for a page the MUI shows in
 * another slot (Now Playing and Song Information are the player and detail slots, not a main view). */
uint32_t mui_door_count(void);
const char* mui_door_label(uint32_t i);
const char* mui_door_line(uint32_t i);
/* I615d: the top bar Browse button's engine lines (the rich UI's PG_BROWSE page load). Home sends mui_door_line(0). */
#define MUI_TOPBAR_BROWSE_LINES 3u
extern const char* const mui_topbar_browse_lines[MUI_TOPBAR_BROWSE_LINES];

/* I792 (drift doc 2026-10-30-mui-mac-drift section 4): how the MUI shows page `page` of the shared page table (uimodel/pages.h). Every page
 * has exactly one entry: a MUI_PAGE_MAIN_VIEW (the main list view, switched by an engine line), a MUI_PAGE_GROUP (the MUI's own page group)
 * or a MUI_PAGE_EXEMPT carrying the written reason it has no slot. kind MUI_PAGE_GAP with no reason is a page nobody mapped: the route
 * loop in tests/ui/mui_contract_test.c fails on it. NULL past SEL_UIM_PG_COUNT. */
enum { MUI_PAGE_GAP = 0, MUI_PAGE_MAIN_VIEW, MUI_PAGE_GROUP, MUI_PAGE_EXEMPT };
typedef struct mui_page_slot_t {
    uint32_t kind;
    const char* exempt; /* MUI_PAGE_EXEMPT: why the page has no MUI slot (and the ledger row that closes it) */
} mui_page_slot_t;
const mui_page_slot_t* mui_page_slot(uint32_t page);

/* I735: the panes of the MUI main slot. MUI_PANE_LIST is the song list every engine-line page (Home, Search, Library, ...) shows; the
 * Visualizer and Pattern pages are own panes, reached from the Mac UI's keys (txt_bindings default: v -> @viz, t -> @pattern; the
 * View menu items carry the same shortcuts) and shown in the main slot at every width. `opens` counts the openings per pane. */
enum { MUI_PANE_LIST = 0, MUI_PANE_VIZ, MUI_PANE_TRACKER, MUI_PANE_COUNT };
typedef struct mui_mainpane {
    uint32_t pane;
    uint32_t opens[MUI_PANE_COUNT];
} mui_mainpane;
uint32_t mui_pane_of_page(uint32_t page);       /* the pane page `page` shows in; MUI_PANE_LIST for a page the list shows */
uint32_t mui_pane_page(uint32_t pane);          /* the shared-table page a pane shows (SEL_UIM_PG_*) */
uint32_t mui_pane_of_action(const char* act);   /* "@viz" -> MUI_PANE_VIZ, "@pattern" -> MUI_PANE_TRACKER; MUI_PANE_COUNT otherwise */
const char* mui_pane_title(uint32_t pane);      /* the View menu item title, NULL for the list */
char mui_pane_shortcut(uint32_t pane);          /* the Mac UI's key for the page, '\0' when none */
/* The page entry: shows the pane of `page`, counts the opening, returns the pane. Any other page (a door or the top bar) goes back
 * to the list through the same call. */
uint32_t mui_pane_open(mui_mainpane* mp, uint32_t page);
/* A key press (txt_key code and mods) as the Mac UI's default table reads it: opens the page it names. MUI_PANE_COUNT when the key opens none. */
uint32_t mui_pane_key(mui_mainpane* mp, uint32_t code, uint32_t mods);

/* MUI-P4-2: Library breadcrumbs. Pieces of a browse path split on "/" and the AmigaDOS ":" (the volume keeps its colon);
 * `end` is the length of the path up to and including the piece, so a click on a crumb opens path[0..end). Returns the
 * piece count even above cap. The last piece equals the engine's browse_leaf. */
typedef struct mui_crumb {
    char name[48];
    uint32_t end;
} mui_crumb;
uint32_t mui_path_crumbs(const char* path, mui_crumb* out, uint32_t cap);

/* MUI-P4-15: which state-text view (sel_uim_state_view) a view source shows its empty line for. */
uint32_t mui_state_view_of(uint32_t source);
/* MUI-P4-15: 1 when the view shows no entry. The Home and Search sources always carry section headings ("Songs (0)",
 * "Recently played"), so row_count alone is never 0 there; they are empty when no row is anything but a heading. */
int mui_view_is_empty(const sel_view_state* v);

/* MUI-P4-11: the one-line status that stands in for a toast. */
typedef struct mui_toast {
    char text[160];
    uint32_t until_ms;
    uint32_t shown;
    int visible;
} mui_toast;
uint32_t mui_toast_ms(int is_error);
void mui_toast_show(mui_toast* t, const char* text, int is_error, uint32_t now_ms);
int mui_toast_tick(mui_toast* t, uint32_t now_ms); /* 1 when it just went away */

/* MUI-P4-14: Help window rows, one per action table row. */
uint32_t mui_help_count(void);
void mui_help_row(uint32_t i, char* out, size_t cap);

/* MUI-P4-3 search: the lines a changed String gadget sends; line 0 is the one search line. */
uint32_t mui_search_lines(const char* text, char lines[3][160]);
/* MUI-P4-4 queue: entry ids, as the desktop queue sends them. */
void mui_queue_line(const char* verb, uint32_t id, uint32_t to_id, char* out, size_t cap); /* "move", "remove", "clear" */
/* MUI-P4-5 playlists: action is "open", "rename" or "delete"; name is only used by rename. Returns 0 for an unknown action. */
uint32_t mui_playlist_line(const char* action, uint32_t id, const char* name, char* out, size_t cap);
/* MUI-P4-6 song menu: the line of item 0..4 (sel_uim_song_menu_label) for the song at path. Play is OPEN and is followed by the
 * line mui_song_menu_follow gives (PLAY); item 3 needs a playlist id; item 4 is QUEUE_REMOVE on the queue page (qid), else
 * LIBRARY_REMOVE. Returns the line length, 0 when the item has no line. */
uint32_t mui_song_menu_line(uint32_t item, const char* path, uint32_t qid, uint32_t playlist_id, int on_queue, char* out, size_t cap);
const char* mui_song_menu_follow(uint32_t item);
/* MUI-P4-7 likes and ratings: the Cycle entry index 0..5 (0 is None) to its line, and the heart. */
void mui_rating_line(uint32_t cycle_index, char* out, size_t cap);
const char* mui_like_line(void);
/* MUI-P4-8 Now Playing: the objects of the group in order, and a synced lyric slot to its line index (-1: none). */
uint32_t mui_np_objects(const char** names, uint32_t cap);
int32_t mui_lyric_line_of(int32_t lyric_slot);
/* MUI-P4-9 song info: the next subsong of a Cycle (wraps after the last) as a SUBSONG line; 0 when there is only one. */
uint32_t mui_subsong_cycle_line(uint32_t subsong, uint32_t count, char* out, size_t cap);
/* MUI-P4-12 dialogs: the confirm text per kind (0 remove, 1 clear library) and the line a confirm sends. */
void mui_confirm_text(uint32_t kind, const char* title, uint32_t songs, uint32_t playlists, char* out, size_t cap);
const char* mui_confirm_line(uint32_t kind);
/* MUI-P4-13 command line: hands the typed line to the engine sender unchanged; returns the sender's result. */
typedef int (*mui_send_fn)(void* ctx, const char* line);
int mui_cmdline_submit(mui_send_fn send, void* ctx, const char* text);
/* MUI-P4-16 tracker: the text of one pattern cell, "C-4 01 .. 0A0F"-style fields joined by spaces. */
void mui_tracker_cell_text(const sel_rp_cell* cell, char* out, size_t cap);

/* MUI-P5-6 drop and clipboard: an AppWindow argument to its line (file QUEUE_ADD, drawer LIBRARY_SCAN); the text of
 * Copy Song Title; its IFF FTXT image for clipboard.device. Each returns the length, 0 when it does not fit or is empty. */
uint32_t mui_path_line(const char* head, const char* path, char* out, size_t cap);
uint32_t mui_drop_line(const char* path, int is_dir, char* out, size_t cap);
uint32_t mui_clip_text(const char* artist, const char* title, char* out, size_t cap);
uint32_t mui_clip_iff(const char* text, uint32_t n, uint8_t* out, size_t cap);


/* One horizontal MUI row is never narrower than its widest labels summed. Thirteen controls (eleven buttons and two cycles) in one
 * row made the window wider than the 800 pixel screen and MUI refused to open it (MUIA_Window_Open stayed 0) once Settings
 * joined them (MUI-P4-10): the first MUI_TOOL_ROW2 buttons stay in row 2, the rest and the cycles go to row 3. */
#define MUI_TOOL_BTNS 12u
#define MUI_TOOL_ROW2 7u
extern const char* const mui_tool_labels[MUI_TOOL_BTNS];
/* Label characters in tool row 2 (row 0) or row 3 (row 1). */
uint32_t mui_tool_row_chars(uint32_t row);
/* Minimum pixel widths of the button rows as mui_ui.c builds them (mui_icons.h width model): tool row 0 or 1, the transport row
 * (worst-case labels over play, repeat and random states; row 0 = the first MUI_TRANSPORT_ROW1 buttons plus the volume slider, row 1 = the state buttons Repeat and Random), the extras row. */
uint32_t mui_tool_row_width(uint32_t row);
uint32_t mui_transport_row_width(uint32_t row);
uint32_t mui_extra_row_width(void);

/* I671/I949b: screen plan and open ladder (map/mui_screen.c). MUI 3.8 refuses a window whose minimum size does not fit the
 * screen (MUIA_Window_Open reads back 0); MUI itself is the measurement, so no height model lives here. The plan keeps the
 * Workbench mode (Selecta never picks an interlaced screen on its own) and sets the depth. */
#define MUI_MODE_LACE 0x00000004u
#define MUI_INVALID_MODE 0xFFFFFFFFu
/* Height of one icon button: icon, vertical padding, frame. */
uint32_t mui_button_height(void);
typedef struct mui_screen_plan_t {
    uint32_t mode;          /* SA_DisplayID for the own screen */
    uint32_t width, height; /* expected screen size */
    uint32_t depth;         /* 8 on a foreign (RTG) mode, 4 on a native one */
} mui_screen_plan_t;
/* Plans the own screen from the Workbench mode and size and its DIPF_IS_FOREIGN flag. Always returns 0 (mode kept).
 * mui_screen_plans counts the calls (sentinel). */
uint32_t mui_screen_plan(uint32_t wb_mode, uint32_t wb_w, uint32_t wb_h, int foreign, mui_screen_plan_t* out);
uint32_t mui_screen_plans(void);
/* I949b open ladder: the layout reference size to try at each step after a refused MUIA_Window_Open. Step 0 is the layout of
 * the screen itself, step 1 the most compact class (width MUI_COMPACT_W, below the 600 px rail breakpoint). laced = the screen
 * is a user-chosen interlaced mode: the layout uses the 256-row metrics (half the rows). Returns 1 with the layout to apply and
 * retry, 0 when no step is left (the caller reports MUI_FAIL_NO_WINDOW). mui_open_ladder_calls counts the calls (sentinel). */
enum { MUI_OPEN_STEPS = 2, MUI_COMPACT_W = 599 };
typedef struct mui_open_step_t {
    uint32_t step;
    uint32_t layout_w, layout_h;
} mui_open_step_t;
uint32_t mui_open_ladder(uint32_t step, uint32_t screen_w, uint32_t screen_h, int laced, mui_open_step_t* out);
uint32_t mui_open_ladder_calls(void);
/* I667n: the Workbench mode is RTG. Picasso96 (uaegfx) modes do not set DIPF_IS_FOREIGN, so the Workbench bitmap decides too: an
 * RTG bitmap is not BMF_STANDARD (planar), and a planar one is never deeper than 8. Inputs are the DisplayInfo flag test, the
 * BMA_FLAGS BMF_STANDARD test and BMA_DEPTH. */
int mui_screen_is_rtg(int dipf_foreign, int bm_standard, uint32_t bm_depth);
/* I675 (D-M4-3): which window attributes the main window gets. On Selecta's own SELECTA screen (the full screen default) it is
 * a borderless backdrop window with no drag bar, depth, size or close gadget, filling the screen; on Workbench (compact mode)
 * it keeps the normal borders. 1 = set the MUIA_Window_* attribute TRUE, 0 = FALSE. titled = 1: the window gets the title
 * "Selecta", 0: MUIA_Window_Title NULL. Intuition draws a title bar on a WFLG_BORDERLESS window whenever it has a title
 * (rig 2 2026-10-29: BorderTop 9, the 9-row band "Selecta" Johan saw), so the borderless window must be untitled.
 * mui_window_attrs_calls counts the calls (sentinel: mui_open must reach it). */
typedef struct mui_window_attrs_t {
    uint8_t borderless, backdrop, dragbar, depthgadget, sizegadget, closegadget, titled;
    uint8_t header; /* I730: 1 = the window draws its own flat header row (title + close button); dialogs on the own screen only */
} mui_window_attrs_t;
void mui_window_attrs(int own_screen, mui_window_attrs_t* out);
uint32_t mui_window_attrs_calls(void);
/* I730: the attributes of every dialog window (Settings, Help, the Commands palette). One path for all of them. On the own
 * SELECTA screen: borderless, untitled (Intuition would add a title bar to a titled borderless window), no drag bar, depth, size
 * or close gadget, not a backdrop (it floats centred over the main window), header = 1 (our own title row with a close button,
 * Esc closes). On Workbench (compact mode): normal borders and title, no own header. Built on mui_window_attrs.
 * mui_dialog_attrs_calls counts the calls (sentinel: every dialog must reach it). */
void mui_dialog_attrs(int own_screen, mui_window_attrs_t* out);
uint32_t mui_dialog_attrs_calls(void);
/* MUI-P8 (plan 2026-10-25-amiga-mui.md section P8): parity rows the first MUI pass left out. Every action here is fired by
 * its shared action-table id (mui_menu_line), the same line the desktop sends; nothing keeps a second list. */
/* Fires the table line of action `id` through send; returns 1 when the line was sent. mui_action_fires() is the call-count
 * sentinel the tests read. */
uint32_t mui_action_fire(const char* id, mui_send_fn send, void* ctx);
uint32_t mui_action_fires(void);
/* MUI-P8-1: the album and speed actions, as transport-area buttons (the menu items and keys come from the table). */
enum { MUI_EXTRA_COUNT = 6 };
const char* mui_extra_action(uint32_t i); /* action id, NULL past the count */
const char* mui_extra_label(uint32_t i);  /* the action's label, the desktop's words */
const char* mui_extra_tip(uint32_t i);    /* the action's tooltip text */

/* MUI-P8-2: Unsolo All Voices and Toggle Filter Light are table actions (ids solo_clear, filter_led: menu and key come
 * from the table). F1..F10 recall and Shift+F1..F10 store a program through the shared model sel_uim_fkey_*. */
typedef struct mui_fkey_bind {
    uint32_t code, mods; /* TXT_KEY_F1.. and TXT_MOD_SHIFT for a store */
    uint32_t slot;       /* 1..10 */
    int store;
} mui_fkey_bind;
/* 20 rows: ten recalls, then ten stores. Returns the row count (may exceed cap; only cap rows are written). */
uint32_t mui_fkeys_build(mui_fkey_bind* out, uint32_t cap);
/* A pressed key: sends the program's lines through send, one call each. Returns the lines sent, 0 for a key that is no
 * program key. */
uint32_t mui_fkey_fire(uint32_t code, uint32_t mods, mui_send_fn send, void* ctx);

/* MUI-P8-3: the Library mode Cycle (Composers, Tree, Columns, Files, History, Sources: the desktop's words, from
 * uimodel/browse.h). mui_libmode_fire sends the mode's lines (window_rows high) and returns how many were sent.
 * MUI-P8-4: the Queue scope Cycle (library, album, playlist); a choice sends SCOPE <name> then re-reads the queue window.
 * mui_view_fires() is the call-count sentinel for both. Visual Layout is the table action layout_visual (menu and key). */
enum { MUI_LIBMODE_COUNT = 6, MUI_SCOPE_COUNT = 3 };
const char* mui_libmode_label(uint32_t i);
uint32_t mui_libmode_fire(uint32_t i, uint32_t window_rows, mui_send_fn send, void* ctx);
const char* mui_scope_label(uint32_t i);
uint32_t mui_scope_fire(uint32_t i, mui_send_fn send, void* ctx);
uint32_t mui_view_fires(void);

/* I670 (design 2026-10-29-desktop-library-architecture.md section 4; MUI plan 2026-10-28-mui-custom-classes.md section 1, "library
 * prev/next"): the sidebar library filter control. A previous (dir < 0) or next (dir > 0) press steps the filter in force through
 * sel_uim_lfilter_step (the desktop chips' clamp), runs sel_uim_lfilter_pass (the desktop's filter pass) into out[0..max) and
 * writes the label of the filter now in force. Returns the new filter and the rows read in *rows. mui_libfilter_presses() counts
 * the presses. */
uint32_t mui_libfilter_press(uint32_t filter, int dir, sel_engine* e, sel_engine_library_item* out, uint32_t max, uint32_t* rows,
                             char* label, uint32_t cap);
/* The same pass for a chip click: filter `filter` is taken as is (a clamped index, mui_libfilter_press steps then calls this). */
uint32_t mui_libfilter_set(uint32_t filter, sel_engine* e, sel_engine_library_item* out, uint32_t max, uint32_t* rows, char* label, uint32_t cap);
uint32_t mui_libfilter_presses(void);

/* MUI-P8-5: the Playlists and Song info buttons; MUI-P8-12: tile size Slider steps; MUI-P8-14: Amiga runtime prefs. All read the
 * shared model (uimodel/viewcmds.h, uimodel/browse.h). Each fire returns how many lines were sent. */
#define MUI_VIEWBTN_COUNT 5u
const char* mui_viewbtn_label(uint32_t i); /* 0 New Folder, 1 New Playlist, 2 Mark Wrong, 3 Skip Song, 4 Save Note */
uint32_t mui_viewbtn_fire(uint32_t i, const char* text, uint32_t folder, mui_send_fn send, void* ctx);
uint32_t mui_tile_steps(void);
int32_t mui_tile_step(uint32_t i);
uint32_t mui_amiga_hotkeys_set(int enabled, mui_send_fn send, void* ctx);
uint32_t mui_amiga_priority_set(int32_t v, mui_send_fn send, void* ctx);
uint32_t mui_amiga_group_set(const char* path, mui_send_fn send, void* ctx);
uint32_t mui_pref_sets(void); /* call-count sentinel: pref sets sent */

/* MUI-P8-13: activate door i. A door with an engine line sends it; Now Playing and Song Information have none and switch the
 * MUI's own page group instead: *page gets the door's page name ("nowplaying", "songinfo"), no engine line is sent. Returns the
 * lines sent; mui_page_switches() counts the page switches. */
uint32_t mui_door_activate(uint32_t i, mui_send_fn send, void* ctx, const char** page);
uint32_t mui_page_switches(void);

/* MUI-P8-6: the Library Statistics panel. The panel text is the shared model's rows ("label: value" lines), Less shows the first
 * SEL_UIM_STATS_LESS, More all of them; NULL stats (Hide) gives an empty text. Returns the bytes written. */
uint32_t mui_stats_text(const char* stats, int more, char* out, uint32_t cap);
uint32_t mui_stats_builds(void); /* call-count sentinel */


/* MUI-P8-7: the Activity, Downloads and Extra Files panes. Ids 0..2 index sel_uim_panel_*; the menu ids (MUIA_UserData of the menu
 * items) are MUI_ID_PANEL_BASE + pane, above every control id P8-5..P8-14 uses. The Activity key is Ctrl+A and toggles that pane. */
#define MUI_ID_PANEL_BASE 0x8700u
#define MUI_PANES 3u
uint32_t mui_panel_menu_id(uint32_t pane);
uint32_t mui_panel_key_pane(uint32_t code, uint32_t mods); /* pane the key opens or closes, MUI_PANES for none */
uint32_t mui_panel_toggle(uint32_t pane);                   /* toggles the pane's open bit, returns the open mask */
uint32_t mui_panel_open_mask(void);
uint32_t mui_panel_text(uint32_t pane, const sel_uim_net* st, char* out, uint32_t cap); /* the pane's rows, one per line */
uint32_t mui_panel_builds(void);                            /* call-count sentinel */

/* MUI-P8-8: Sources page and sync offer, search source. Every fire returns the lines sent. */
uint32_t mui_source_toggle(const sel_uim_source* s, uint32_t n, uint32_t flip, mui_send_fn send, void* ctx); /* off list, then SOURCES */
uint32_t mui_source_sync(const sel_uim_source* s, mui_send_fn send, void* ctx);                                 /* LIBRARY_SYNC, then SOURCES */
const char* mui_source_status(int pending, int failed, int64_t synced);
uint32_t mui_offer_not_now(int64_t now, mui_send_fn send, void* ctx);                                         /* Not now button */
uint32_t mui_offer_sync(const sel_uim_source* s, uint32_t n, uint32_t ticked, mui_send_fn send, void* ctx);   /* Sync button */
const char* mui_search_source_label(uint32_t i);
uint32_t mui_search_source_set(uint32_t i, mui_send_fn send, void* ctx);
uint32_t mui_source_fires(void); /* call-count sentinel */

/* MUI-P8-9: info line pieces. */
int32_t mui_info_mode_key(char key);
uint32_t mui_info_scroll_next(uint32_t offset, uint32_t text_len, int no_scroll);
const char* mui_info_ahi(int active);
uint32_t mui_info_downloaded(const sel_uim_net* st, char* out, uint32_t cap);
const char* mui_list_kept_cell(int kept); /* column 0 of a list row */

/* MUI-P8-10: MUIA_ShortHelp texts. Transport button i for the frame: the action's tip text with its key hint (sel_uim_action_tip_text);
 * sidebar door i: the door label. mui_tooltip_hover copies the text to the status line and counts the update. */
uint32_t mui_tooltip_transport(uint32_t button, const sel_uim_frame* f, char* out, uint32_t cap);
uint32_t mui_tooltip_door(uint32_t i, char* out, uint32_t cap);
uint32_t mui_tooltip_hover(const char* tip, char* status, uint32_t cap);
uint32_t mui_tooltip_hovers(void);

/* I798 (O1): the command palette window model. The rows are the shared txt_palette (the Mac's command table, the default key
 * bindings); `row[i]` is the text of match i. mui_palette_filter narrows to the query, mui_palette_pick runs the verb through
 * `send` or, when the verb needs arguments, writes "verb " to `fill` for the command line (the Mac palette_pick). */
typedef struct mui_palette {
    txt_bindings binds;
    txt_palette pal;
    char row[SEL_CMD_COUNT][96];
    char query[TXT_PAL_QUERY];
    char last_verb[48];
    uint32_t count;
    uint32_t opens, runs, fills; /* sentinels */
} mui_palette;
enum { MUI_PAL_NONE = 0, MUI_PAL_RUN, MUI_PAL_FILL, MUI_PAL_REJECTED };
void mui_palette_open(mui_palette* p);
void mui_palette_filter(mui_palette* p, const char* query);
const char* mui_palette_row(void* ctx, long row);
uint32_t mui_palette_pick(mui_palette* p, long row, mui_send_fn send, void* ctx, char* fill, uint32_t cap);

/* I798 (O3): the one tooltip text function the MUI controls read. Transport and doors go through the P8-10 mapping (action tip
 * with key hint, door label); the like and Now Playing panel texts are the Mac's (sel_uim_tip_*). mui_tip_calls is the sentinel. */
enum { MUI_TIP_TRANSPORT = 0, MUI_TIP_DOOR, MUI_TIP_LIKE, MUI_TIP_NP_PANEL, MUI_TIP_SEARCH, MUI_TIP_CMD, MUI_TIP_PALETTE, MUI_TIP_SEEK, MUI_TIP_VOLUME };
uint32_t mui_tip(uint32_t kind, uint32_t index, const sel_uim_frame* f, int flag, char* out, uint32_t cap);
uint32_t mui_tip_calls(void);

/* I798 (T2, R3): flip the Now Playing column of the window layout (Mac I619 detail_hidden); returns the new hidden flag.
 * mui_np_toggles counts the flips. */
uint32_t mui_np_toggle(mui_layout_map_t* m);
uint32_t mui_np_toggles(void);

/* MUI-P8-11: remembered state. close stores geometry, open panes, visualiser and browse view as PREF_SET lines through `send`;
 * open reads them back through `get` (the pref value of a key, NULL when unset). */
typedef struct mui_remember {
    int32_t x, y;
    uint32_t w, h;
    uint32_t panes;
    char viz[32];
    char browse[32];
} mui_remember;
typedef const char* (*mui_pref_get_fn)(void* ctx, const char* key);
uint32_t mui_remember_close(const mui_remember* r, mui_send_fn send, void* ctx);
uint32_t mui_remember_open(mui_remember* r, mui_pref_get_fn get, void* ctx); /* returns how many of the four items were found */

/* I793 (drift doc S3 to S9): the Your Library sidebar model, built from the uimodel texts and the engine's library items. The MUI
 * sidebar (mui_ui.c mui_sidebar_group) builds its objects from this and nothing else; mui_sidebar_builds() is the sentinel. */
#define MUI_SIDEBAR_DOORS 8u
#define MUI_SIDEBAR_ROWS 16u
#define MUI_RAIL_SPACING 1 /* I949c: pixels between the rail doors */
typedef struct mui_sidebar_in {
    int top_bar;     /* the screen tier has the top bar (the doors Home and Search leave) */
    uint32_t chip;   /* 0 All, 1..SEL_UIM_SIDE_CHIPS a kind chip */
    uint32_t by_you_only; /* the By You sub-chip of Playlists */
    uint32_t wide;   /* Expand is on */
    int bitmap;      /* the bitmap tier */
    int rail;        /* I949c: the compact class rail: doors only (see mui_layout_compact) */
    int chips_single; /* I669: the chips do not fit the sidebar, S5 is the one label with the chevrons (mui_sidebar_chips_single) */
    const char* filter;
    const sel_engine_library_item* items;
    uint32_t item_n;
} mui_sidebar_in;
typedef struct mui_sidebar_row {
    char mark[4], title[96], subtitle[96], type[16];
    uint32_t kind, id;
} mui_sidebar_row;
typedef struct mui_sidebar_model {
    char brand[16], brand_icon[8];            /* I797 (S1): the "Selecta" title and the logo icon key; the icon is "" on the bitmap tier (text only) */
    uint32_t door[MUI_SIDEBAR_DOORS], door_n; /* indexes of the shown doors in the uimodel door table */
    char title[24], create[24], expand[24];   /* the header: Your Library, Create Playlist, Expand/Collapse Library */
    char chip[SEL_UIM_SIDE_CHIPS][16];
    int chip_shown[SEL_UIM_SIDE_CHIPS];
    uint32_t chip_selected;
    int chip_clear, by_you;
    int chip_single;                          /* I669: S5 is one label (chip_label) between chevrons; chip_shown is all 0 */
    char chip_left_name[20];                  /* I831: "Previous Filter" or, with a filter in force, "Clear Filter" (sel_uim_lfilter_left_name) */
    int chip_left_clear;                      /* I831: the left button of the label control is the x (sel_uim_lfilter_left_clears) */
    char chip_label[16];                      /* the name of the filter in force (sel_uim_lfilter_label) */
    mui_sidebar_row row[MUI_SIDEBAR_ROWS];
    uint32_t row_n, cols;
    int wide;
    char empty[32];                           /* "" when no empty state shows */
    int rail;                                 /* I949c: doors only; brand, header, chips, rows, empty state and card are all empty */
    int card;
    char card_title[40], card_body[48], card_button[24];
} mui_sidebar_model;
void mui_sidebar_build(const mui_sidebar_in* in, mui_sidebar_model* out);
/* I669: 1 when the kind chips do not fit a sidebar `sidebar_px` wide (or on the bitmap tier); the decision is sel_uim_lfilter_chips_single. */
int mui_sidebar_chips_single(int bitmap, uint32_t sidebar_px);
uint32_t mui_sidebar_builds(void);
/* I949c: sentinel, the builds that took the rail path (no Your Library block). */
uint32_t mui_sidebar_rail_builds(void);

#endif
