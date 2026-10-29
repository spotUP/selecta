/* I648: Selecta look for MUI 3.8 objects, derived from the desktop UI theme tokens (tokens.h, txt_theme_default). Pure C99.
 * Plan 2026-10-25-amiga-mui.md section "MUI theme". Backgrounds are MUI RGB image specs, frames are MUIV_Frame_* numbers;
 * text pens stay the user's MUI pens (MUI 3.8 has no RGB text pen), so the look is applied only where it keeps the text
 * readable (mui_theme_readable). No custom drawing: MUI draws the background and frame itself. */
#ifndef SELECTA_MUI_THEME_H
#define SELECTA_MUI_THEME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mui_theme_role {
    MUI_TH_WINDOW,  /* window contents: BACKGROUND, no frame */
    MUI_TH_PANEL,   /* sidebar, detail, player groups: SURFACE, flat */
    MUI_TH_BUTTON,  /* buttons: SURFACE_RAISED, thin frame */
    MUI_TH_FIELD,   /* string gadget, cycle: SURFACE, string frame */
    MUI_TH_LIST,    /* list views: SURFACE, thin frame */
    MUI_TH_STATUS,  /* text lines: PANEL base, flat */
    MUI_TH_COUNT
} mui_theme_role;

/* Same numbers as MUIV_Frame_* in MUI 3.8 libraries/mui.h (checked by the native build). */
enum { MUI_FRAME_NONE = 0, MUI_FRAME_BUTTON = 1, MUI_FRAME_TEXT = 3, MUI_FRAME_STRING = 4, MUI_FRAME_INPUTLIST = 6 };

typedef struct mui_theme_look {
    char background[40]; /* MUIA_Background spec "2:rrrrrrrr,gggggggg,bbbbbbbb" */
    uint32_t frame;      /* MUIA_Frame value */
    uint32_t bg_rgb;     /* 0xRRGGBB the spec was made from */
} mui_theme_look;

/* tokens: the SEL_GUI_TOKEN_COUNT array of a txt_theme. Fills *out and counts one application; returns 0 on bad input. */
/* MUIA_Background spec string "2:rrrrrrrr,gggggggg,bbbbbbbb" for a 0xRRGGBB colour (cap >= 40). */
void mui_theme_bg_spec(uint32_t rgb, char* out, size_t cap);
int mui_theme_look_for(mui_theme_role role, const uint32_t* tokens, mui_theme_look* out);
/* The theme stays off when the user's MUI text pen (0xRRGGBB) would not be readable on the role background: dark text on a
 * dark Selecta surface. WCAG-style ratio in integer tenths, at least 40 (4.0) is readable. */
int mui_theme_readable(uint32_t text_rgb, uint32_t bg_rgb);
/* True when aa_text draws fg itself: not the plain backend, and either a truecolour screen or fg readable on bg. */
int mui_theme_text_uses_token(int truecolour, int plain, uint32_t fg_rgb, uint32_t bg_rgb);
/* One decision for the whole look. MUI 3.8 draws object text with the screen DrawInfo pens only (the escape code ESC followed by
 * a pen number picks that pen; there is no RGB text pen), so the look needs one of those pens to be readable on every role
 * background. pen_rgb: the n screen pen colours (0xRRGGBB, index = DrawInfo pen number); tokens as in mui_theme_look_for.
 * Returns the pen number nearest to the theme TEXT colour that is readable on BACKGROUND, SURFACE, SURFACE_RAISED and PANEL, or
 * -1: the look then stays off for every role and the user's MUI look is left alone. */
int mui_theme_pick_pen(const uint32_t* pen_rgb, uint32_t n, const uint32_t* tokens);
/* Count of mui_theme_look_for calls that succeeded since start (sentinel for the build path test). */
uint32_t mui_theme_applied(void);
uint32_t mui_theme_applied_role(mui_theme_role role);

#ifdef __cplusplus
}
#endif
#endif
