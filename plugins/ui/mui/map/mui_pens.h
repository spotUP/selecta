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
    MUI_PEN_TEXT_MUTED, /* I672: secondary labels (artist, times, hints), the desktop TEXT_MUTED token; pen 7 of design section 4 */
    MUI_PEN_COUNT
};

/* AmigaOS DrawInfo pen count (DRI_VERSION 2: DETAILPEN .. BARTRIMPEN). */
enum { MUI_DRI_PENS = 12 };

/* I672: which label draws in which palette slot. Primary text keeps the TEXT token; the labels the desktop UI draws in
 * TEXT_MUTED (artists, albums, times, hints, section labels) take the muted slot. One table, mui_ui.c routes through it. */
typedef enum {
    MUI_LBL_TITLE = 0,
    MUI_LBL_ARTIST,
    MUI_LBL_ALBUM,
    MUI_LBL_CLOCK,
    MUI_LBL_REMAINING,
    MUI_LBL_LENGTH,
    MUI_LBL_FORMAT,
    MUI_LBL_STATUS,
    MUI_LBL_SECTION,  /* "Next in Queue" */
    MUI_LBL_UPCOMING, /* queue preview rows */
    MUI_LBL_COUNT
} mui_label;

typedef struct mui_pens_plan {
    uint32_t rgb[MUI_PEN_COUNT];     /* 0xRRGGBB per slot */
    uint32_t rgb32[MUI_PEN_COUNT][3]; /* the same as 32-bit left-justified components (Amiga SetRGB32 / ObtainBestPen) */
    int16_t dri_pens[MUI_DRI_PENS + 1]; /* SA_Pens array: pen number per DrawInfo pen, terminated by -1 */
} mui_pens_plan;

/* tokens: the SEL_GUI_TOKEN_COUNT array of a txt_theme. Fills *out and counts one plan; returns 0 on bad input. */
int mui_pens_plan_for(const uint32_t* tokens, mui_pens_plan* out);
/* Palette slot (MUI_PEN_TEXT or MUI_PEN_TEXT_MUTED) a label draws in; MUI_PEN_TEXT for an unknown label. */
int mui_pens_label_slot(mui_label label);
/* DrawInfo pen number 0..7 that the plan points at palette slot `slot` (the digit of a MUI text pen escape), or -1. The first
 * match wins, so DETAILPEN carries the muted slot and TEXTPEN stays the TEXT slot. */
int mui_pens_dri_index(const mui_pens_plan* plan, int slot);
/* Count of mui_pens_plan_for calls that succeeded since start (sentinel for the build path test). */
uint32_t mui_pens_planned(void);

#ifdef __cplusplus
}
#endif
#endif
