#include "mui_theme.h"

#include <selecta/uimodel/tokens.h>

#include <stdio.h>

static uint32_t g_applied[MUI_TH_COUNT];

int mui_theme_look_for(mui_theme_role role, const uint32_t* tokens, mui_theme_look* out) {
    uint32_t rgb;
    uint32_t frame = MUI_FRAME_NONE;
    if (tokens == NULL || out == NULL || (unsigned)role >= MUI_TH_COUNT) {
        return 0;
    }
    switch (role) {
    case MUI_TH_WINDOW:
        rgb = tokens[SEL_GUI_TOKEN_BACKGROUND];
        break;
    case MUI_TH_PANEL:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE];
        break;
    case MUI_TH_BUTTON:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE_RAISED];
        frame = MUI_FRAME_TEXT;
        break;
    case MUI_TH_FIELD:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE];
        frame = MUI_FRAME_STRING;
        break;
    case MUI_TH_LIST:
        rgb = tokens[SEL_GUI_TOKEN_SURFACE];
        frame = MUI_FRAME_TEXT;
        break;
    default: /* MUI_TH_STATUS */
        rgb = tokens[SEL_GUI_TOKEN_PANEL];
        break;
    }
    /* MUI RGB image spec: each 8-bit component replicated to 32 bits. */
    snprintf(out->background, sizeof out->background, "2:%08x,%08x,%08x", (unsigned)(((rgb >> 16) & 0xFFu) * 0x01010101u),
             (unsigned)(((rgb >> 8) & 0xFFu) * 0x01010101u), (unsigned)((rgb & 0xFFu) * 0x01010101u));
    out->frame = frame;
    out->bg_rgb = rgb;
    g_applied[role]++;
    return 1;
}

/* Relative luminance in 0..255000 (integer Rec.709 weights, no gamma: enough for a go/no-go gate). */
static uint32_t lum(uint32_t rgb) {
    return ((rgb >> 16) & 0xFFu) * 2126u / 10u + ((rgb >> 8) & 0xFFu) * 7152u / 10u + (rgb & 0xFFu) * 722u / 10u;
}

int mui_theme_readable(uint32_t text_rgb, uint32_t bg_rgb) {
    uint32_t a = lum(text_rgb) + 12750u; /* +0.05 of full scale */
    uint32_t b = lum(bg_rgb) + 12750u;
    uint32_t hi = a > b ? a : b;
    uint32_t lo = a > b ? b : a;
    return hi * 10u / lo >= 40u;
}

uint32_t mui_theme_applied(void) {
    uint32_t n = 0;
    for (int i = 0; i < MUI_TH_COUNT; i++) {
        n += g_applied[i];
    }
    return n;
}

uint32_t mui_theme_applied_role(mui_theme_role role) {
    return (unsigned)role < MUI_TH_COUNT ? g_applied[role] : 0;
}
