/* Selecta_AAButton (I665 spike, I667a base, I667b rollout; design 2026-10-28-mui-custom-classes.md section 3.1): MUI 3.8 Area
 * subclass that draws EVERY Selecta button: a rounded rectangle in ONE flat colour (theme token by role and state, picked by
 * mui_aa_button_rgb), an optional baked Lucide icon and an antialiased caption (mui_aa_draw). Amiga only; the colour, ramp and
 * metric maths are in map/mui_aa.c and tested on the host. */
#ifndef MUI_AABTN_CLASS_H
#define MUI_AABTN_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

typedef struct mui_aabtn_spec {
    const char* caption;  /* shown text; also the ShortHelp unless help is set */
    const char* widest;   /* longest caption the button will show (sets the minimum width); NULL = fit: the width follows
                           * every caption mui_aabtn_set_caption gives it */
    const char* help;     /* ShortHelp, NULL = caption */
    int icon;             /* sel_gui_icon id of the icon, -1 none */
    int alt_icon;         /* icon swapped in by mui_aabtn_set_icon, -1 none (kept for the page-group buttons: play/pause) */
    int icon_only;        /* no caption drawn (dense rows); the caption stays the ShortHelp */
    int role;             /* MUI_AA_ROLE_NORMAL, _ACCENT or _GHOST */
    LONG id;              /* MUIM_Application_ReturnID on release */
    int selected;         /* role toggle: initial checked state */
    LONG weight;          /* MUIA_Weight: 100 shares the row, 0 sizes to the contents */
    int align;            /* MUI_AA_ALIGN_*: where icon+caption sit in the button (I667b4); MUI_AA_ALIGN_CENTRE for every button but the sidebar doors */
} mui_aabtn_spec;

Object* mui_aabtn_new(const mui_aabtn_spec* spec);
/* New caption (state labels); redraws. */
void mui_aabtn_set_caption(Object* obj, const char* caption);
/* Which of the spec's two icons is shown (0 icon, 1 alt_icon); redraws. */
void mui_aabtn_set_alt(Object* obj, int alt);
/* Lit state of a momentary button (Repeat on, Shuffle on): drawn with the SELECTED face; redraws. */
void mui_aabtn_set_on(Object* obj, int on);
/* Which backend drew the last frame: 0 none yet, 1 plain MUI text (fallback), 2 ttengine truecolour, 3 ttengine palette ramp. */
int mui_aabtn_backend(Object* obj);
/* Buttons constructed since start (sentinel: equals the number of buttons the window build made). */
uint32_t mui_aabtn_made(void);
void mui_aabtn_class_free(void);

#endif
