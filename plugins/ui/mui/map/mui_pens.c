#include "mui_pens.h"

#include <stddef.h>

#include <selecta/uimodel/tokens.h>

static uint32_t g_planned;

int mui_pens_plan_for(const uint32_t* tokens, mui_pens_plan* out) {
    /* DrawInfo pen order: DETAIL, BLOCK, TEXT, SHINE, SHADOW, FILL, FILLTEXT, BACKGROUND, HILIGHTTEXT, BARDETAIL, BARBLOCK, BARTRIM.
     * Text pens point at the soft grey TEXT token, backgrounds at BACKGROUND, 3D light and shade at DIVIDER and BACKGROUND. */
    static const int16_t dri[MUI_DRI_PENS + 1] = {
        MUI_PEN_TEXT,   MUI_PEN_PANEL,      MUI_PEN_TEXT,  MUI_PEN_DIVIDER, MUI_PEN_BACKGROUND, MUI_PEN_RAISED, MUI_PEN_TEXT,
        MUI_PEN_BACKGROUND, MUI_PEN_TEXT, MUI_PEN_TEXT, MUI_PEN_PANEL,    MUI_PEN_BACKGROUND, -1};
    int i;
    if (tokens == NULL || out == NULL) {
        return 0;
    }
    out->rgb[MUI_PEN_BACKGROUND] = tokens[SEL_GUI_TOKEN_BACKGROUND];
    out->rgb[MUI_PEN_TEXT] = tokens[SEL_GUI_TOKEN_TEXT];
    out->rgb[MUI_PEN_SURFACE] = tokens[SEL_GUI_TOKEN_SURFACE];
    out->rgb[MUI_PEN_RAISED] = tokens[SEL_GUI_TOKEN_SURFACE_RAISED];
    out->rgb[MUI_PEN_PANEL] = tokens[SEL_GUI_TOKEN_PANEL];
    out->rgb[MUI_PEN_ACCENT] = tokens[SEL_GUI_TOKEN_ACCENT];
    out->rgb[MUI_PEN_DIVIDER] = tokens[SEL_GUI_TOKEN_DIVIDER];
    for (i = 0; i < MUI_PEN_COUNT; i++) {
        out->rgb32[i][0] = ((out->rgb[i] >> 16) & 0xFFu) * 0x01010101u;
        out->rgb32[i][1] = ((out->rgb[i] >> 8) & 0xFFu) * 0x01010101u;
        out->rgb32[i][2] = (out->rgb[i] & 0xFFu) * 0x01010101u;
    }
    for (i = 0; i <= MUI_DRI_PENS; i++) {
        out->dri_pens[i] = dri[i];
    }
    g_planned++;
    return 1;
}

uint32_t mui_pens_planned(void) {
    return g_planned;
}
