/* I665 / I662 spike: pure geometry and colour picks for the flat rounded button and its antialiased caption (aa_draw).
 * No Amiga calls, so the host test reaches them. Plan 2026-10-25-amiga-mui.md section "MUI theme (I648, in-code half)":
 * every colour comes from a theme token; this module only derives from them. */
#ifndef SELECTA_MUI_AA_H
#define SELECTA_MUI_AA_H

#include <stdint.h>

#include <selecta/uimodel/tokens.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pixels left uncovered at the left and right end of row y (0 = top) of a w x h rounded rectangle with corner radius r
 * (clamped to h/2 and w/2). Row centres are sampled against the corner circle; the middle rows return 0. */
int mui_aa_round_inset(int w, int h, int r, int y);
/* Ramp slot (0 = background .. steps-1 = full foreground) for an 8-bit glyph coverage; steps >= 2. */
int mui_aa_ramp_index(int coverage, int steps);
/* Colour (0xRRGGBB) of ramp slot i between bg (slot 0) and fg (slot steps-1). */
uint32_t mui_aa_ramp_rgb(uint32_t fg, uint32_t bg, int steps, int i);
/* Pressed face derived from the raised face token: darker on a light face, lighter on a dark one (no bevel pens). */
uint32_t mui_aa_pressed_rgb(uint32_t face);

/* I667a (design 2026-10-28-mui-custom-classes.md section 2): shared state, tier and metric tables of every Selecta_AA class.
 * Colours come from the token array (txt_theme.token), never from literals here. */
enum { MUI_AA_NORMAL, MUI_AA_HOVER, MUI_AA_PRESSED, MUI_AA_DISABLED, MUI_AA_SELECTED };
enum { MUI_AA_ROLE_NORMAL, MUI_AA_ROLE_ACCENT, MUI_AA_ROLE_GHOST };
/* Screen tiers by rows (section 3): S 640x256 and 320x256, M up to 600 rows, L up to 800, XL above. */
enum { MUI_AA_TIER_S, MUI_AA_TIER_M, MUI_AA_TIER_L, MUI_AA_TIER_XL };
/* Coverage ramp kinds (section 4): AGA four held token pens, RTG eight blended pens, TC none (ttengine blends). */
enum { MUI_AA_RAMP_AGA, MUI_AA_RAMP_RTG, MUI_AA_RAMP_TC };
#define MUI_AA_RAMP_MAX 8

typedef struct mui_aa_metrics_t {
    int font_px, pad_x, pad_y, icon_px, icon_gap, height, radius;
} mui_aa_metrics_t;

/* Face colour of a control in a state: normal = face, hover = SURFACE_HOVER, pressed and selected = SURFACE_PRESSED,
 * disabled = the normal face. */
uint32_t mui_aa_state_rgb(const uint32_t* token, uint32_t face, int state);
/* Caption colour: text, or TEXT_DISABLED when disabled. */
uint32_t mui_aa_text_rgb(const uint32_t* token, uint32_t text, int state);
/* Face and caption of a button role in a state (accent: ACCENT and ACCENT_TEXT; ghost: PANEL, selected SURFACE_RAISED). */
void mui_aa_button_rgb(const uint32_t* token, int role, int state, uint32_t* face, uint32_t* text);
/* Number of times the three functions above ran: the sentinel that the classes (and not a copy) pick the colours. */
uint32_t mui_aa_state_calls(void);
/* Coverage ramp for a kind into out[] (>= MUI_AA_RAMP_MAX); returns the slot count. AGA: face, TEXT_DISABLED, TEXT_MUTED, TEXT
 * (fg is ignored); RTG: mui_aa_ramp_rgb steps; TC: 0 slots. */
int mui_aa_ramp_pens(int kind, const uint32_t* token, uint32_t fg, uint32_t bg, uint32_t* out);
/* Slot of an 8-bit coverage in a ramp kind (AGA quarters, RTG eight steps, TC always the top slot). */
int mui_aa_ramp_slot(int kind, int coverage);
/* Font pixel size for the screen: 13 up to 600 rows, 15 up to 800, 17 above, 0 (bitmap topaz 8) at 256 rows or less. */
int mui_aa_font_px(int screen_rows, int screen_cols);
int mui_aa_tier(int screen_rows);
/* Control heights and paddings of one tier table for a font size from mui_aa_font_px. */
void mui_aa_metrics(int font_px, mui_aa_metrics_t* out);

#ifdef __cplusplus
}
#endif
#endif
