/* Selecta_AAList (I667i, design 2026-10-28-mui-custom-classes.md section 3.6, popup look 3.3). MUI 3.8 Area subclass with its own row model
 * (a count and a row callback): SURFACE face, TEXT rows, SURFACE_HOVER hover row, SURFACE_PRESSED selected row, a vertical pad above the first
 * and below the last row, text at the left pad, wheel and keys. mui_aalist_new returns a horizontal group of the list and its AAScroller
 * (3.5), notifications wired both ways, so no stock Prop gadget is drawn. AAL_Picked notifies on a click press over a row or Return (the
 * row). Amiga only; the maths is in map/mui_aa.c (mui_aa_list_*). */
#ifndef MUI_AALIST_CLASS_H
#define MUI_AALIST_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>

#define AAL_Count (TAG_USER + 0x53410501UL)  /* LONG: rows (notifies) */
#define AAL_Active (TAG_USER + 0x53410502UL) /* LONG: selected row, -1 none (notifies) */
#define AAL_First (TAG_USER + 0x53410503UL)  /* LONG: first visible row (notifies) */
#define AAL_Picked (TAG_USER + 0x53410504UL) /* LONG: row chosen by click or Return (notifies every time) */

#define AAL_Columns (TAG_USER + 0x53410505UL) /* ptr to mui_aalist_cols, copied (I667i2): the shown columns, may change while live */
#define AAL_Playing (TAG_USER + 0x53410506UL) /* LONG: row whose first column is drawn in ACCENT, -1 none (I667i2) */

#define MUI_AALIST_COLS 5

/* AAM_GetRow (I713): the read-back method for tools that drive the window from outside (the rig's wasabid lists a row as an item and
 * double-clicks it by its text). The class has no public parent that knows its rows, so this is its documented accessor. The caller fills
 * row and text/cap; the method returns TRUE when the row exists and sets: text = the cell texts of the row joined by two spaces (empty
 * cells left out, truncated to cap), x/y/w/h = the row's box in window coordinates (all 0 when the row is scrolled out of view or the
 * list is not shown), selected = 1 on the active row. */
#define AAM_GetRow (TAG_USER + 0x53410510UL)
typedef struct mui_aalist_getrow {
    ULONG MethodID; /* AAM_GetRow */
    LONG row;
    char* text;
    LONG cap;
    LONG x, y, w, h;
    LONG selected;
} mui_aalist_getrow;

typedef const char* (*mui_aalist_row_fn)(void* ctx, long row);
/* Cell text of a multi-column list; row -1 asks the column title (the title row). */
typedef const char* (*mui_aalist_cell_fn)(void* ctx, long row, int col);

/* Columns: weight > 0 shares the free width by weight, weight 0 is as wide as sample[k] (NULL = empty). */
typedef struct mui_aalist_cols {
    int n;
    int weight[MUI_AALIST_COLS];
    const char* sample[MUI_AALIST_COLS];
} mui_aalist_cols;

typedef struct mui_aalist_spec {
    long count, active;
    mui_aalist_row_fn row;
    void* ctx;
    int max_rows; /* default height in rows (0 = 8) */
    mui_aalist_cell_fn cell; /* I667i2: multi-column rows (used instead of row) */
    const mui_aalist_cols* cols;
    int title;       /* a title row above the rows (cell asked for row -1) */
    int double_pick; /* AAL_Picked only on a double click or Return (a single click just selects) */
    int no_select;   /* lyrics: no hover, no selection, wheel only */
} mui_aalist_spec;

/* The group (list + scroller). *list_out receives the list object (the one that carries the AAL_* attributes). */
Object* mui_aalist_new(const mui_aalist_spec* spec, Object** list_out);
/* Lists constructed since start (sentinel). */
uint32_t mui_aalist_made(void);
void mui_aalist_class_free(void);

#endif
