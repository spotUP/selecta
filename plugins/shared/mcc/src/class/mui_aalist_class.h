/* Selecta_AAList (I667i, design 2026-10-28-mui-custom-classes.md section 3.6, popup look 3.3). MUI 3.8 Area subclass with its own row model
 * (a count and a row callback): SURFACE face, TEXT rows, SURFACE_HOVER hover row, SURFACE_PRESSED selected row, a vertical pad above the first
 * and below the last row, text at the left pad, wheel and keys. UP_NewList returns a horizontal group of the list and its AAScroller
 * (3.5), notifications wired both ways, so no stock Prop gadget is drawn. UPA_List_Picked notifies on a click press over a row or Return (the
 * row). Amiga only; the maths is in map/mui_aa.c (mui_aa_list_*). */
#ifndef MUI_AALIST_CLASS_H
#define MUI_AALIST_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

#define MUI_AALIST_COLS UP_LIST_COLS
typedef UP_ListColumns mui_aalist_cols;
typedef UP_ListGetRow mui_aalist_getrow;

typedef const char* (*mui_aalist_row_fn)(void* ctx, long row);
/* Cell text of a multi-column list; row -1 asks the column title (the title row). */
typedef const char* (*mui_aalist_cell_fn)(void* ctx, long row, int col);

typedef struct mui_aalist_spec {
    long count, active;
    mui_aalist_row_fn row;
    void* ctx;
    int max_rows; /* default height in rows (0 = 8) */
    mui_aalist_cell_fn cell; /* I667i2: multi-column rows (used instead of row) */
    const mui_aalist_cols* cols;
    int title;       /* a title row above the rows (cell asked for row -1) */
    int double_pick; /* UPA_List_Picked only on a double click or Return (a single click just selects) */
    int no_select;   /* lyrics: no hover, no selection, wheel only */
} mui_aalist_spec;

/* Selecta wrapper (design 8.3): builds the UPA_List_* tag list from the spec (mui_aalist_tags), owns the two hook structs, calls UP_NewList.
 * The group is list + scroller; *list_out receives the list object (the one that carries the UPA_List_* attributes). */
Object* mui_aalist_new(const mui_aalist_spec* spec, Object** list_out);
/* Lists constructed since start (sentinel). */
uint32_t mui_aalist_made(void);
void mui_aalist_class_free(void);

#endif
