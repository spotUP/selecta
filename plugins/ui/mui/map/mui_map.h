/* MUI plugin mapping functions (plan 2026-10-25-amiga-mui.md section 2.2). Pure C: no <proto/muimaster.h>, so the
 * host builds and tests this directory (MUI-P1-1). */
#ifndef SEL_MUI_MAP_H
#define SEL_MUI_MAP_H

#include <stddef.h>
#include <stdint.h>

#include <selecta/engine.h>
#include <selecta/uimodel.h>

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
enum { MUI_SLOT_SUGGESTION = 0, MUI_SLOT_SIDEBAR, MUI_SLOT_MAIN, MUI_SLOT_DETAIL, MUI_SLOT_PLAYER, MUI_SLOT_COUNT };
typedef struct mui_layout_map_t {
    uint32_t width, height;
    uint32_t breakpoint; /* SEL_UIM_BP_* from sel_uim_breakpoint_for */
    uint8_t show[MUI_SLOT_COUNT];
    uint32_t weight_w[MUI_SLOT_COUNT];
    uint32_t weight_h[MUI_SLOT_COUNT];
    uint8_t balance_sidebar_main, balance_main_detail; /* Balance object between two shown panels */
} mui_layout_map_t;
void mui_layout_map(mui_layout_map_t* m, uint32_t width, uint32_t height);
/* Recompute for a new window size. Returns 1 when the breakpoint changed (the caller re-applies ShowMe, *flips counts
 * the slots whose visibility changed), 0 when only weights move. */
uint32_t mui_layout_relayout(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t* flips);

/* MUI-P2-5: the transport row. Six buttons; the action each sends depends on the frame (Play/Pause on the play state,
 * Repeat cycles off, all, one, Random toggles). Labels and ids come from the action table, so the text is the full
 * English words of the desktop UI. Seek is a Slider 0..MUI_SEEK_MAX bound to progress_q16, volume 0..MUI_VOLUME_MAX. */
enum { MUI_BTN_PREV = 0, MUI_BTN_PLAY, MUI_BTN_NEXT, MUI_BTN_STOP, MUI_BTN_REPEAT, MUI_BTN_RANDOM, MUI_BTN_COUNT };
#define MUI_SEEK_MAX 65536
#define MUI_VOLUME_MAX 100
/* Action id the button sends for this frame; NULL for an unknown button. */
const char* mui_transport_action(uint32_t button, const sel_uim_frame* f);
/* Button label: the action's label for this frame. */
const char* mui_transport_label(uint32_t button, const sel_uim_frame* f);

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
/* Tree style: the engine line a click on a node row sends (VIEW_EXPAND/VIEW_COLLAPSE <node id>); "" for a non-node. */
void mui_list_toggle_line(const sel_view_row* r, char* out, size_t cap);
/* Drilldown breadcrumb "Queue by folder  Drilldown > a > b" from the shared title function. */
void mui_list_breadcrumb(const sel_view_state* v, char* out, size_t cap);

enum { MUI_COL_TITLE = 0, MUI_COL_ARTIST, MUI_COL_ALBUM, MUI_COL_LENGTH, MUI_COL_FORMAT, MUI_COL_COUNT };
/* Visible columns in display order for a SEL_UIM_BP_*; hidden by width: format first, then album, then length. */
uint32_t mui_columns_for(uint32_t breakpoint, uint8_t out[MUI_COL_COUNT]);
/* MUIA_List_Format string for those columns: "BAR," per column but the last. */
void mui_columns_format(uint32_t breakpoint, char* out, size_t cap);
/* Cell text of one row; "" for the columns the engine row does not carry (the view model rows hold title and artist). */
const char* mui_row_cell(const sel_view_row* r, uint32_t column);
/* MUI-P3-4: the album, length and format cells of one row. The engine rows carry only title and artist, so these come from the
 * snapshot: the format is the file extension of the row's song (queue rows: its queue entry's path, else the row text); the
 * length is the open song's duration on its own queue row, else the shared unknown clock "--:--" (the engine keeps no length per
 * queued song); the album is what the caller got from sel_engine_song_group for the row's path ("" when uncatalogued). */
typedef struct mui_row_extra {
    char album[64];
    char length[16];
    char format[16];
} mui_row_extra;
void mui_row_extra_fill(const sel_state* st, const sel_view_row* r, const char* album, mui_row_extra* out);

/* MUI-P4-1: Home rows. SEL_ROW_HOME rows carry `key` plus `album` (song to play versus album to open) and `composer`. */
typedef enum mui_home_kind { MUI_HOME_NONE = 0, MUI_HOME_SONG, MUI_HOME_ALBUM, MUI_HOME_COMPOSER } mui_home_kind;
mui_home_kind mui_home_kind_of(const sel_view_row* r);
/* The three engine lines a click on window row `row` sends (cursor, activate, ask the home source again, as the
 * desktop shelf tile does); returns 3, or 0 for a row that is not a Home entry. */
uint32_t mui_home_lines(const sel_view_state* v, uint32_t row, char lines[3][48]);

/* MUI-P4-0: the sidebar doors, built from the shared uimodel door table (include/selecta/uimodel/doors.h). The label is the
 * table's; the line is the engine line that switches the main view to the door's page, NULL for a page the MUI shows in
 * another slot (Now Playing and Song Information are the player and detail slots, not a main view). */
uint32_t mui_door_count(void);
const char* mui_door_label(uint32_t i);
const char* mui_door_line(uint32_t i);

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
#define MUI_TOOL_BTNS 11u
#define MUI_TOOL_ROW2 7u
extern const char* const mui_tool_labels[MUI_TOOL_BTNS];
/* Label characters in tool row 2 (row 0) or row 3 (row 1). */
uint32_t mui_tool_row_chars(uint32_t row);
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

#endif
