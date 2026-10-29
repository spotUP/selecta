#include "mui_pens.h"

#include <stddef.h>

#include <selecta/uimodel/tokens.h>

#include "mui_icons.h"

static uint32_t g_planned;

int mui_pens_plan_for(const uint32_t* tokens, mui_pens_plan* out) {
    /* DrawInfo pen order: DETAIL, BLOCK, TEXT, SHINE, SHADOW, FILL, FILLTEXT, BACKGROUND, HILIGHTTEXT, BARDETAIL, BARBLOCK, BARTRIM.
     * Text pens point at the soft grey TEXT token (DETAILPEN, unused by MUI text, carries TEXT_MUTED for I672 escapes), backgrounds at BACKGROUND, 3D light and shade at DIVIDER and BACKGROUND. */
    static const int16_t dri[MUI_DRI_PENS + 1] = {
        MUI_PEN_TEXT_MUTED, MUI_PEN_PANEL,      MUI_PEN_TEXT,  MUI_PEN_DIVIDER, MUI_PEN_BACKGROUND, MUI_PEN_RAISED, MUI_PEN_TEXT,
        MUI_PEN_BACKGROUND, MUI_PEN_TEXT, MUI_PEN_TEXT, MUI_PEN_PANEL,    MUI_PEN_BACKGROUND, -1};
    int i;
    uint32_t icon[12];
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
    out->rgb[MUI_PEN_TEXT_MUTED] = tokens[SEL_GUI_TOKEN_TEXT_MUTED];
    out->rgb[MUI_PEN_TEXT_DISABLED] = tokens[SEL_GUI_TOKEN_TEXT_DISABLED];
    out->rgb[MUI_PEN_TRACK] = tokens[SEL_GUI_TOKEN_TRACK];
    out->rgb[MUI_PEN_SURFACE_HOVER] = tokens[SEL_GUI_TOKEN_SURFACE_HOVER];
    out->rgb[MUI_PEN_SURFACE_PRESSED] = tokens[SEL_GUI_TOKEN_SURFACE_PRESSED];
    out->rgb[MUI_PEN_FOCUS_RING] = tokens[SEL_GUI_TOKEN_FOCUS_RING];
    mui_icon_palette(tokens, 1, icon); /* pens 1-3 of the 4 pen icon palette are the opaque ones */
    for (i = 0; i < MUI_PEN_ICON_COUNT; i++) {
        out->rgb[MUI_PEN_ICON_FIRST + i] = ((icon[3 + i * 3] >> 24) << 16) | ((icon[4 + i * 3] >> 24) << 8) | (icon[5 + i * 3] >> 24);
    }
    for (i = 0; i < MUI_PEN_TOTAL; i++) {
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

int mui_pens_label_slot(mui_label label) {
    switch (label) {
    case MUI_LBL_ARTIST:
    case MUI_LBL_ALBUM:
    case MUI_LBL_CLOCK:
    case MUI_LBL_REMAINING:
    case MUI_LBL_LENGTH:
    case MUI_LBL_FORMAT:
    case MUI_LBL_STATUS:
    case MUI_LBL_SECTION:
    case MUI_LBL_UPCOMING:
        return MUI_PEN_TEXT_MUTED;
    default:
        return MUI_PEN_TEXT;
    }
}

int mui_pens_dri_index(const mui_pens_plan* plan, int slot) {
    int i;
    if (plan == NULL) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        if (plan->dri_pens[i] == slot) {
            return i;
        }
    }
    return -1;
}

int mui_pens_aga_slot(int token) {
    switch (token) {
    case SEL_GUI_TOKEN_BACKGROUND: return MUI_PEN_BACKGROUND;
    case SEL_GUI_TOKEN_TEXT: return MUI_PEN_TEXT;
    case SEL_GUI_TOKEN_SURFACE: return MUI_PEN_SURFACE;
    case SEL_GUI_TOKEN_SURFACE_RAISED: return MUI_PEN_RAISED;
    case SEL_GUI_TOKEN_PANEL: return MUI_PEN_PANEL;
    case SEL_GUI_TOKEN_ACCENT: return MUI_PEN_ACCENT;
    case SEL_GUI_TOKEN_DIVIDER: return MUI_PEN_DIVIDER;
    case SEL_GUI_TOKEN_TEXT_MUTED: return MUI_PEN_TEXT_MUTED;
    case SEL_GUI_TOKEN_TEXT_DISABLED: return MUI_PEN_TEXT_DISABLED;
    case SEL_GUI_TOKEN_TRACK: return MUI_PEN_TRACK;
    case SEL_GUI_TOKEN_SURFACE_HOVER: return MUI_PEN_SURFACE_HOVER;
    case SEL_GUI_TOKEN_SURFACE_PRESSED: return MUI_PEN_SURFACE_PRESSED;
    case SEL_GUI_TOKEN_FOCUS_RING: return MUI_PEN_FOCUS_RING;
    case SEL_GUI_TOKEN_HOVER_LIGHT:
    case SEL_GUI_TOKEN_ACCENT_TEXT: return MUI_PEN_TEXT;
    case SEL_GUI_TOKEN_HEADER_TINT: return MUI_PEN_PANEL;
    default: return -1;
    }
}
