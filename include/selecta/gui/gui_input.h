/* Desktop input events and the scripted input line parser (plan 2.8, DUI-P3-3). Integers only.
 * One struct for every event the backends deliver and the `--input <file>` script feeds. */
#ifndef SELECTA_GUI_INPUT_H
#define SELECTA_GUI_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sel_gui_input_kind {
    SEL_GUI_IN_NONE = 0,
    SEL_GUI_IN_KEY,    /* code (TXT_KEY_* or a code point), mods (TXT_MOD_*) */
    SEL_GUI_IN_CLICK,  /* x, y, button (0 primary, 1 secondary), clicks (1 single, 2 a second click in a row) */
    SEL_GUI_IN_WHEEL,  /* dx, dy in Q16 notches */
    SEL_GUI_IN_PINCH,  /* scale as Q16 (1.25 = 81920) */
    SEL_GUI_IN_DROP,   /* path */
    SEL_GUI_IN_RESIZE, /* x = width, y = height */
    SEL_GUI_IN_WAIT,   /* ms */
    SEL_GUI_IN_MOVE,   /* x, y: the pointer moved; code 1 while the primary button is held, then x2, y2 is the press point */
    SEL_GUI_IN_DRAG,   /* x, y to x2, y2: press, move, release */
    SEL_GUI_IN_SWIPE   /* x = +1 swipe right (Back), -1 swipe left (Forward) */
} sel_gui_input_kind;

#define SEL_GUI_INPUT_PATH 256u

typedef struct sel_gui_input {
    uint32_t kind; /* sel_gui_input_kind */
    uint32_t code;
    uint32_t mods;
    int32_t x, y;
    int32_t x2, y2;  /* SEL_GUI_IN_DRAG: where the button was released */
    uint32_t button; /* SEL_GUI_IN_CLICK: 0 primary, 1 secondary */
    uint32_t clicks; /* SEL_GUI_IN_CLICK: click count */
    int32_t dx, dy; /* Q16 */
    int32_t scale;  /* Q16 */
    uint32_t ms;
    char path[SEL_GUI_INPUT_PATH];
} sel_gui_input;

/* Parses one script line (`key ctrl+f`, `click 640 120 [count]`, `dclick 640 120`, `rclick 640 120`, `move 640 120`, `drag 10 20 30 40`, `wheel 0 -3`, `pinch 1.25`, `swipe left|right`, `drop /a.mod`, `resize 900 600`,
 * `wait 200`). Returns 1 and fills *ev, or 0 for a blank, comment or unknown line. */
int sel_gui_input_parse(const char* line, sel_gui_input* ev);

/* Parses a key spec: modifiers joined by '+' (ctrl, alt, shift) then a name (enter, space, tab, escape, backspace, up,
 * down, left, right, home, end, pageup, pagedown, insert, delete, f1..f12) or one character. Returns 1 on success. */
int sel_gui_key_parse(const char* spec, uint32_t* code, uint32_t* mods);

/* Live pointer and touchpad input (I242). A backend reads its window events and hands plain integers to
 * sel_gui_input_map, which builds the same sel_gui_input the `--input` script builds, so the shell has one path from
 * pointer events to the mouse, tooltip and gesture code. No SDL type is needed here. */
typedef enum sel_gui_raw_kind {
    SEL_GUI_RAW_NONE = 0,
    SEL_GUI_RAW_BUTTON_DOWN, /* x, y, button (1 left, 2 middle, 3 right, 4 back, 5 forward: SDL numbering), clicks */
    SEL_GUI_RAW_BUTTON_UP,   /* the same fields */
    SEL_GUI_RAW_MOTION,      /* x, y */
    SEL_GUI_RAW_WHEEL,       /* dx, dy in Q16 notches (positive dy scrolls up) */
    SEL_GUI_RAW_PINCH,       /* scale Q16: the change since the last update, above Q16 one is zoom in */
    SEL_GUI_RAW_SWIPE,       /* dx: positive swipes right (Back), negative left (Forward) */
    SEL_GUI_RAW_KEY,         /* code (TXT_KEY_* or a code point), mods (TXT_MOD_*): a key press, same space as sel_gui_key_parse */
    SEL_GUI_RAW_TEXT         /* code: one typed code point, already layout and shift translated; no modifiers */
} sel_gui_raw_kind;

typedef struct sel_gui_raw {
    uint32_t kind; /* sel_gui_raw_kind */
    int32_t x, y;
    uint32_t button, clicks;
    int32_t dx, scale, dy;
    uint32_t code, mods; /* SEL_GUI_RAW_KEY and SEL_GUI_RAW_TEXT */
} sel_gui_raw;

/* Press state between a button down and its up. */
typedef struct sel_gui_input_map_state {
    int32_t down_x, down_y;
    uint32_t down_button; /* 0 when no button is held */
} sel_gui_input_map_state;

/* Pixels the pointer may travel between press and release for the release to still be a click. */
#define SEL_GUI_DRAG_SLOP 4

/* Translates one raw event. Returns 1 and fills *out, or 0 when the raw event produces nothing (a button down, which
 * waits for its up; the middle button; a zero wheel; a pinch of scale 0 or less; an unknown kind). Left and right up
 * give SEL_GUI_IN_CLICK (button 0 and 1, the click count passed through, at least 1), or SEL_GUI_IN_DRAG when the
 * pointer moved more than SEL_GUI_DRAG_SLOP since the press. A key or text raw event gives SEL_GUI_IN_KEY with the same
 * code and mods the `key` script line builds (text: mods 0; a zero code gives nothing). Back and forward up give SEL_GUI_IN_SWIPE (+1 and -1). */
int sel_gui_input_map(sel_gui_input_map_state* st, const sel_gui_raw* raw, sel_gui_input* out);

#ifdef __cplusplus
}
#endif
#endif
