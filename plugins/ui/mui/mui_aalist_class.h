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

typedef const char* (*mui_aalist_row_fn)(void* ctx, long row);

typedef struct mui_aalist_spec {
    long count, active;
    mui_aalist_row_fn row;
    void* ctx;
    int max_rows; /* default height in rows (0 = 8) */
} mui_aalist_spec;

/* The group (list + scroller). *list_out receives the list object (the one that carries the AAL_* attributes). */
Object* mui_aalist_new(const mui_aalist_spec* spec, Object** list_out);
/* Lists constructed since start (sentinel). */
uint32_t mui_aalist_made(void);
void mui_aalist_class_free(void);

#endif
