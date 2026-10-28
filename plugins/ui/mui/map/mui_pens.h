/* I666: the Selecta palette for MUI 3.8, derived from the desktop UI theme tokens (tokens.h). Pure C99, no Amiga headers.
 * Root cause it fixes: MUI maps every colour it is asked for ("2:" RGB backgrounds, DrawInfo text pens) to the nearest pen the
 * screen already has. Selecta opened on the Workbench screen (16 colours on AGA, a crowded shared palette on RTG), which holds no
 * dark greys and no soft grey, so surfaces collapsed to black and text to white. The fix is a screen Selecta owns: this module
 * decides which token colours must exist there and at which pen numbers, mui_ui.c opens the screen with that palette and
 * DrawInfo pens, and obtains every colour at exact precision so MUI finds exact matches. Plan 2026-10-25-amiga-mui.md section
 * "MUI theme (I648, in-code half)". */
#ifndef SELECTA_MUI_PENS_H
#define SELECTA_MUI_PENS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Palette slots. Pen number = slot number on the Selecta screen (the palette is loaded in this order). */
enum {
    MUI_PEN_BACKGROUND = 0,
    MUI_PEN_TEXT,
    MUI_PEN_SURFACE,
    MUI_PEN_RAISED,
    MUI_PEN_PANEL,
    MUI_PEN_ACCENT,
    MUI_PEN_DIVIDER,
    MUI_PEN_COUNT
};

/* AmigaOS DrawInfo pen count (DRI_VERSION 2: DETAILPEN .. BARTRIMPEN). */
enum { MUI_DRI_PENS = 12 };

typedef struct mui_pens_plan {
    uint32_t rgb[MUI_PEN_COUNT];     /* 0xRRGGBB per slot */
    uint32_t rgb32[MUI_PEN_COUNT][3]; /* the same as 32-bit left-justified components (Amiga SetRGB32 / ObtainBestPen) */
    int16_t dri_pens[MUI_DRI_PENS + 1]; /* SA_Pens array: pen number per DrawInfo pen, terminated by -1 */
} mui_pens_plan;

/* tokens: the SEL_GUI_TOKEN_COUNT array of a txt_theme. Fills *out and counts one plan; returns 0 on bad input. */
int mui_pens_plan_for(const uint32_t* tokens, mui_pens_plan* out);
/* Count of mui_pens_plan_for calls that succeeded since start (sentinel for the build path test). */
uint32_t mui_pens_planned(void);

#ifdef __cplusplus
}
#endif
#endif
