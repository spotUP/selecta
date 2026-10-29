/* Selecta_AAPanel (I667e, design 2026-10-28-mui-custom-classes.md section 3.8): MUI 3.8 Group subclass that paints the rounded face
 * of every Selecta panel (sidebar, player, main, detail, art frame) in a theme token colour by role (PANEL, SURFACE, SURFACE_RAISED),
 * corner radius RADIUS_LARGE (half on tier S), over the window BACKGROUND, then lets the super class draw the children. Inner
 * padding and spacing are the SPACING token. Replaces the MUI_TH_PANEL groups. Amiga only; the role/radius maths is in map/mui_aa.c. */
#ifndef MUI_AAPANEL_CLASS_H
#define MUI_AAPANEL_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>

typedef struct mui_aapanel_spec {
    const uint32_t* tokens; /* theme token array (SEL_GUI_TOKEN_*) */
    int role;               /* MUI_AA_PANEL_PANEL, _SURFACE or _RAISED */
    int fixed_px;           /* > 0: fixed width and height with no inner padding (the art frame) */
} mui_aapanel_spec;

/* A panel; children are added with OM_ADDMEMBER. */
Object* mui_aapanel_new(const mui_aapanel_spec* spec);
/* Panels constructed since start (sentinel: equals the number of panel sites the window build made). */
uint32_t mui_aapanel_made(void);
void mui_aapanel_class_free(void);

#endif
