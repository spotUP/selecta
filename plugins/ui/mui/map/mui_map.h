/* MUI plugin mapping functions (plan 2026-10-25-amiga-mui.md section 2.2). Pure C: no <proto/muimaster.h>, so the
 * host builds and tests this directory (MUI-P1-1). */
#ifndef SEL_MUI_MAP_H
#define SEL_MUI_MAP_H

#include <stdint.h>

/* The view styles the MUI UI offers. An RTG screen can host the custom canvas (grid, cover flow), so every style;
 * a planar screen has only native list widgets, so the classic list styles (plan section 1, finish line 3). */
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

#endif
