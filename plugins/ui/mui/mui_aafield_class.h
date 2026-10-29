/* Selecta_AAField (I667h, design 2026-10-28-mui-custom-classes.md section 3.4): MUI 3.8 Group subclass holding one stock MUIC_String with
 * no frame. The group paints the flat rounded field (face SURFACE, 1 px outline DIVIDER, FOCUS_RING while the string is the active
 * object) before the children, so every string gadget of the Selecta MUI UI loses the stock string frame and background. MUIA_String_Contents
 * set and get are forwarded to the inner string, so callers treat the field as the string. Amiga only; the maths is in map/mui_aa.c. */
#ifndef MUI_AAFIELD_CLASS_H
#define MUI_AAFIELD_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>

typedef struct mui_aafield_spec {
    const uint32_t* tokens; /* theme token array (SEL_GUI_TOKEN_*) */
    int maxlen;
    int max_w; /* I615d: cap the field's MaxWidth in px (the top bar field); 0 = unbounded */
    long id; /* MUIM_Application_ReturnID on MUIA_String_Acknowledge; 0 = no notification */
} mui_aafield_spec;

Object* mui_aafield_new(const mui_aafield_spec* spec);
/* Fields constructed since start (sentinel: the number of string sites the window build made). */
uint32_t mui_aafield_made(void);
void mui_aafield_class_free(void);

#endif
