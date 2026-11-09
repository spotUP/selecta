/* Selecta_AACycle (I667g, design 2026-10-28-mui-custom-classes.md section 3.3). MUI 3.8 Area subclass replacing MUIC_Cycle: a rounded
 * SURFACE_RAISED face with the active entry in TEXT and a TEXT_MUTED chevron, drawn through aa_round_fill and held pens. A click opens
 * a borderless popup window under the object holding the entries; a pick closes it. Up and Down step the active entry with wrap while
 * the object is the active one. UPA_Cycle_Active notifies like MUIA_Cycle_Active. Amiga only; colours, sizes and the step maths are in
 * map/mui_aa.c. */
#ifndef MUI_AACYCLE_CLASS_H
#define MUI_AACYCLE_CLASS_H

#include <mcc/up.h>
#include <stdint.h>


typedef struct mui_aacycle_spec {
    const char* const* entries; /* NULL terminated */
    long active;
    long id; /* MUIM_Application_ReturnID on every UPA_Cycle_Active change; 0 = none (the wrapper adds the notify) */
} mui_aacycle_spec;

Object* mui_aacycle_new(const mui_aacycle_spec* spec);
/* Cycles constructed since start (sentinel: the number of cycle sites the window build made). */
uint32_t mui_aacycle_made(void);
void mui_aacycle_class_free(void);

#endif
