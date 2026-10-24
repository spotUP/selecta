/* UI model shared by every UI that draws regions (desktop, custom Amiga UI): the frame strings, transport flags,
 * progress in Q16 and one dirty bit per region, built from sel_engine_state() once per pump (plan
 * 2026-10-24-desktop-ui.md DUI-P1-4, M4 plan section 2). Plain C99: no float, no allocation, fixed buffers. */
#ifndef SELECTA_UIMODEL_H
#define SELECTA_UIMODEL_H

#include <selecta/engine.h>

SEL_EXTERN_C_BEGIN

/* Regions of a frame. A widget redraws only when its region's bit is set. */
enum {
    SEL_UIM_DIRTY_SONG = 1u,      /* title, artist, format */
    SEL_UIM_DIRTY_QUEUE = 2u,     /* entry count, playing entry */
    SEL_UIM_DIRTY_TRANSPORT = 4u, /* play state, previous and next availability */
    SEL_UIM_DIRTY_PROGRESS = 8u,  /* elapsed and remaining clock, progress bar */
    SEL_UIM_DIRTY_VOLUME = 16u,
    SEL_UIM_DIRTY_STATUS = 32u    /* status line */
};
#define SEL_UIM_REGIONS 6u
#define SEL_UIM_DIRTY_ALL ((1u << SEL_UIM_REGIONS) - 1u)

typedef struct sel_uim_frame {
    char title[96];
    char artist[64];
    char format[24];
    char clock[16];     /* elapsed, "m:ss" */
    char remaining[16]; /* "-m:ss", empty when the duration is unknown */
    char status[96];
    sel_play_state play;
    uint32_t can_prev, can_next;
    uint32_t progress_q16; /* 0 .. 65536, 0 when the duration is unknown */
    uint32_t queue_count, queue_playing;
    int32_t volume;
} sel_uim_frame;

/* Frame layouts, the values of the pref ui.desktop.mode (full, window, mini). */
enum { SEL_UIM_LAYOUT_FULL = 0, SEL_UIM_LAYOUT_WINDOW = 1, SEL_UIM_LAYOUT_MINI = 2 };

typedef struct sel_uim {
    sel_uim_frame frame;
    uint32_t layout;      /* SEL_UIM_LAYOUT_* */
    uint32_t dirty;       /* regions changed since sel_uim_take_dirty */
    uint32_t last_taken;  /* regions of the latest frame that had any to redraw */
    uint32_t updates;     /* sel_uim_update calls */
} sel_uim;

/* Empty model with every region dirty (the first frame draws everything). */
void sel_uim_init(sel_uim* uim);
/* Rebuild the frame from the engine snapshot; returns the regions whose content changed in this update. */
uint32_t sel_uim_update(sel_uim* uim, const sel_state* st);
/* Select the layout by its pref value; returns 0 for an unknown name. A layout change dirties every region. */
int sel_uim_set_layout(sel_uim* uim, const char* mode);
/* The regions to redraw; clears them. */
uint32_t sel_uim_take_dirty(sel_uim* uim);

/* Display strings and panel models (lifted from the text UI, DUI-P1-5); every UI formats these the same way. */
/* "mm:ss", or "--:--" when `known` is 0. */
void sel_uim_clock(char* out, size_t cap, uint32_t ms, int known);
/* "Playing", "Paused" or "Stopped". */
const char* sel_uim_play_word(sel_play_state p);
/* Appends the mixer stage settings that differ from the defaults ("  LED Filter On", "  Stereo Width 40"). */
void sel_uim_stage_status(char* buf, size_t cap, const sel_state* s);
/* Queue heading: the entry count, or the grouping key and the drilldown breadcrumb. `library` names a query view
 * "Library" instead of "Query". */
void sel_uim_queue_title(char* buf, size_t cap, const sel_view_state* v, int library);
/* A tag name as a label: the META_ prefix dropped, underscores to spaces, lower case after the first letter. */
void sel_uim_tag_label(uint32_t tag, char* out, size_t cap);
/* Semitone 0..95 of a Q8 pitch value (piano-roll rows). */
uint32_t sel_uim_semitone(uint32_t pitch_q8);
/* The block glyph (U+2581..U+2588) for a Q15 level. */
uint32_t sel_uim_level_glyph(uint32_t q15);
/* Pattern note cell: "C-4", "^^^" (note off), "===" (cut), "..." (empty); out holds 4 bytes. */
void sel_uim_note_name(uint8_t n, char* out);

/* Action table (plan DUI-P1-6). */
typedef struct sel_uim_action {
    const char* id;
    const char* label;   /* full English words */
    const char* tooltip;
    const char* icon;    /* icon id, lucide name */
    const char* menu;    /* "Menu/Item" path */
    const char* key;     /* default key name of keybind.c, "" when none */
    const char* line;    /* protocol line: a COMMANDS verb, or PREF_SET of a prefs.def key */
} sel_uim_action;

uint32_t sel_uim_action_count(void);
const sel_uim_action* sel_uim_action_at(uint32_t i);
uint32_t sel_uim_exemption_count(void);
const char* sel_uim_exemption_at(uint32_t i, const char** reason);
/* One help overlay row per action: menu path and default key. */
void sel_uim_help_row(uint32_t i, char* out, size_t cap);
/* 0 when every line is a COMMANDS verb or a PREF_SET of a prefs.def key and every verb has a row or an exemption. */
int sel_uim_actions_check(char* msg, size_t cap);

/* Layout tree and breakpoints (plan DUI-P1-7). A canvas width picks the breakpoint; the regions tile the canvas. */
enum {
    SEL_UIM_BP_THREE_COLUMN = 0,   /* 1200 wide and up */
    SEL_UIM_BP_TWO_COLUMN = 1,     /* 900 to 1199 */
    SEL_UIM_BP_ICON_SIDEBAR = 2,   /* 700 to 899 */
    SEL_UIM_BP_MINI_SUGGESTION = 3, /* below 700: main list plus a banner suggesting the mini layout */
    SEL_UIM_BP_COUNT = 4
};
#define SEL_UIM_LAYOUT_REGIONS 5u

typedef struct sel_uim_region {
    const char* name;
    int32_t x, y;
    uint32_t w, h;
    uint32_t min_w, min_h;
} sel_uim_region;

typedef struct sel_uim_layout {
    uint32_t width, height;
    uint32_t breakpoint; /* SEL_UIM_BP_* */
    uint32_t count;
    sel_uim_region regions[SEL_UIM_LAYOUT_REGIONS];
} sel_uim_layout;

const char* sel_uim_breakpoint_name(uint32_t bp);
uint32_t sel_uim_breakpoint_for(uint32_t width);
void sel_uim_layout_compute(sel_uim_layout* l, uint32_t width, uint32_t height);
int sel_uim_layout_check(const sel_uim_layout* l, char* msg, size_t cap);
/* Hit test (DUI-P3-2): the index of the region of the layout tree that holds the point (x, y) in layout units, or -1
 * outside every region. The regions tile the canvas, so a point belongs to at most one. */
int sel_uim_hit(const sel_uim_layout* l, int32_t x, int32_t y);

/* Effects governor (plan DUI-P1-9): what the canvas format can afford. INDEX8 runs no tween, blur or shadow. */
enum { SEL_UIM_FMT_RGBA32 = 0, SEL_UIM_FMT_INDEX8 = 1 };
enum { SEL_UIM_FX_TWEEN = 1, SEL_UIM_FX_BLUR = 2, SEL_UIM_FX_SHADOW = 3 };

typedef struct sel_uim_budget {
    uint32_t format; /* SEL_UIM_FMT_* */
    uint32_t tweens, blur, shadow; /* 1 when the effect may run */
} sel_uim_budget;

/* Budget for a backend format name ("rgba32", "index8"; NULL means rgba32); returns 0 for an unknown name. */
int sel_uim_budget_for(sel_uim_budget* b, const char* format);
/* 1 when the effect (SEL_UIM_FX_*) may run. Every effect asks before it starts. */
int sel_uim_budget_allows(const sel_uim_budget* b, uint32_t effect);
/* 1 when tweens may run: the format allows them and the pref ui.desktop.reduce_motion is not set (DUI-P3-5). */
int sel_uim_motion_allowed(const sel_uim_budget* b, int reduce_motion);

SEL_EXTERN_C_END

#endif /* SELECTA_UIMODEL_H */
