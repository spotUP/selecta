#ifndef SELECTA_GUI_EDIT_H
#define SELECTA_GUI_EDIT_H
#include <stddef.h>

/* I1243: the one-line text field edit core: a caret, an optional selection (anchor to caret), word and select-all, copy,
 * and an insert that replaces the selection. Pure byte logic over a caller buffer (printable ASCII, as the desktop
 * fields hold); no drawing, no clipboard, so the host test drives it without a window. `seen` is the length the state
 * was last valid for: when the buffer changed behind its back (a restored query), sel_gui_edit_sync puts the caret at the end. */
typedef struct sel_gui_edit {
    size_t anchor, caret, seen;
} sel_gui_edit;

void sel_gui_edit_sync(sel_gui_edit* e, size_t len);           /* clamp, or move to the end when len differs from `seen` */
int sel_gui_edit_has(const sel_gui_edit* e);                   /* 1 when a range is selected */
void sel_gui_edit_range(const sel_gui_edit* e, size_t* lo, size_t* hi);
void sel_gui_edit_set(sel_gui_edit* e, size_t anchor, size_t caret, size_t len);
void sel_gui_edit_all(sel_gui_edit* e, size_t len);
void sel_gui_edit_word(sel_gui_edit* e, const char* buf, size_t len, size_t pos); /* word at pos, for a two-click select */
void sel_gui_edit_move(sel_gui_edit* e, size_t len, int dir, int extend);         /* Left/Right; a plain move collapses a selection to its edge */
/* Replaces the selection (or inserts at the caret) with text; bytes outside 32..126 and `reject` are dropped, the buffer
 * never passes cap - 1. Returns 1 when the buffer changed. */
int sel_gui_edit_insert(sel_gui_edit* e, char* buf, size_t* len, size_t cap, char reject, const char* text, size_t n);
int sel_gui_edit_backspace(sel_gui_edit* e, char* buf, size_t* len);  /* deletes the selection, else the byte before the caret */
size_t sel_gui_edit_copy(const sel_gui_edit* e, const char* buf, char* out, size_t cap); /* the selected text, NUL-terminated; 0 when none */

#endif
