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
    SEL_GUI_IN_MOVE,   /* x, y: the pointer moved */
    SEL_GUI_IN_DRAG    /* x, y to x2, y2: press, move, release */
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

/* Parses one script line (`key ctrl+f`, `click 640 120 [count]`, `dclick 640 120`, `rclick 640 120`, `move 640 120`, `drag 10 20 30 40`, `wheel 0 -3`, `pinch 1.25`, `drop /a.mod`, `resize 900 600`,
 * `wait 200`). Returns 1 and fills *ev, or 0 for a blank, comment or unknown line. */
int sel_gui_input_parse(const char* line, sel_gui_input* ev);

/* Parses a key spec: modifiers joined by '+' (ctrl, alt, shift) then a name (enter, space, tab, escape, backspace, up,
 * down, left, right, home, end, pageup, pagedown, insert, delete, f1..f12) or one character. Returns 1 on success. */
int sel_gui_key_parse(const char* spec, uint32_t* code, uint32_t* mods);

#ifdef __cplusplus
}
#endif
#endif
