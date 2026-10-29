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
/* TOGGLE (I667b-2, AACheck): checkbox box; off = SURFACE face (desktop checkbox), on (state SELECTED) = ACCENT face, ink ACCENT_TEXT. */
enum { MUI_AA_ROLE_NORMAL, MUI_AA_ROLE_ACCENT, MUI_AA_ROLE_GHOST, MUI_AA_ROLE_TOGGLE };
/* Screen tiers by rows (section 3): S 640x256 and 320x256, M up to 600 rows, L up to 800, XL above. */
enum { MUI_AA_TIER_S, MUI_AA_TIER_M, MUI_AA_TIER_L, MUI_AA_TIER_XL };
/* Coverage ramp kinds (section 4): AGA four held token pens, RTG eight blended pens, TC none (ttengine blends). */
enum { MUI_AA_RAMP_AGA, MUI_AA_RAMP_RTG, MUI_AA_RAMP_TC };
#define MUI_AA_RAMP_MAX 8

typedef struct mui_aa_metrics_t {
    int font_px, pad_x, pad_y, icon_px, icon_gap, height, radius, check_px; /* check_px: AACheck box edge (16, tier S 12) */
} mui_aa_metrics_t;

/* Face colour of a control in a state: normal = face, hover = SURFACE_HOVER, pressed and selected = SURFACE_PRESSED,
 * disabled = the normal face. */
uint32_t mui_aa_state_rgb(const uint32_t* token, uint32_t face, int state);
/* Caption colour: text, or TEXT_DISABLED when disabled. */
uint32_t mui_aa_text_rgb(const uint32_t* token, uint32_t text, int state);
/* Face and caption of a button role in a state (accent: ACCENT and ACCENT_TEXT; ghost: PANEL, selected SURFACE_RAISED). */
void mui_aa_button_rgb(const uint32_t* token, int role, int state, uint32_t* face, uint32_t* text);
/* AACheck box paint plan (design 3.1, desktop d_checkbox): ops painted in order into the check_px square, each a rounded fill inset
 * from the box edge. Op 0 fills the square corners with the parent background (SURFACE = MUI_TH_PANEL); checked = one ACCENT
 * box; off = TEXT_MUTED outline (TEXT_DISABLED when disabled) then the face inside it. Corner radius = RADIUS_SMALL (tier S half). */
typedef struct mui_aa_check_op_t {
    int inset, radius;
    uint32_t rgb;
} mui_aa_check_op_t;
#define MUI_AA_CHECK_OPS 3
int mui_aa_check_plan(const uint32_t* token, const mui_aa_metrics_t* m, int state, mui_aa_check_op_t* out);
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

/* I667d (design 3.7, AAText): text tones, alignment and the ellipsis cut. Pure maths, tested on the host. */
enum { MUI_AA_TONE_NORMAL, MUI_AA_TONE_MUTED, MUI_AA_TONE_TITLE };
enum { MUI_AA_ALIGN_LEFT, MUI_AA_ALIGN_CENTRE, MUI_AA_ALIGN_RIGHT };
#define MUI_AA_TITLE_EXTRA_PX 4
/* SEL_GUI_TOKEN_* a tone draws in: normal and title TEXT, muted TEXT_MUTED. */
int mui_aa_tone_token(int tone);
/* Font pixel size of a tone for the screen base size from mui_aa_font_px: title = base + 4, others = base; the bitmap tier (0) stays 0. */
int mui_aa_tone_font_px(int tone, int base_px);
/* Left edge of content of width cw in a box of width w for an alignment (never left of 0). */
int mui_aa_align_x(int align, int w, int cw);
/* I667b4: left edge of a button's icon+caption content of width cw in a button of width w: the alignment inside the box inset by
 * pad_x on both sides (mui_aa_align_x); centre = (w - cw) / 2, left = pad_x, right = w - cw - pad_x. */
int mui_aa_btn_content_x(int align, int w, int cw, int pad_x);
/* Ink width of the first len bytes of s, supplied by the drawing side (ttengine pixmap on the Amiga, an advance table in the test). */
typedef int (*mui_aa_width_fn)(void* ctx, const char* s, int len);
/* Text that fits max_w: s itself when it fits, else the longest prefix plus "..." that fits (just "..." when not even one character
 * does, empty when max_w cannot hold the dots). out gets at most cap-1 bytes; returns the length written. */
int mui_aa_text_fit(const char* s, int max_w, mui_aa_width_fn width, void* ctx, char* out, int cap);
/* I667f3: the widest content a text line asks for. A fixed line (the seek row time labels) takes only its default width defw so the
 * seek slider gets the rest of the row; any other line is unbounded (MUI_AA_TEXT_UNBOUNDED, the class maps it to MUI_MAXMAX). */
#define MUI_AA_TEXT_UNBOUNDED (-1)
int mui_aa_text_maxw(int fixed, int defw);

/* I667e (design 3.8, AAPanel): rounded panel groups. Role -> face token (panel PANEL, surface SURFACE, raised SURFACE_RAISED),
 * corner radius RADIUS_LARGE (tier S, font_px 0: half), inner padding and spacing SPACING. Tested on the host. */
enum { MUI_AA_PANEL_PANEL, MUI_AA_PANEL_SURFACE, MUI_AA_PANEL_RAISED, MUI_AA_PANEL_ROLES };
int mui_aa_panel_token(int role);
uint32_t mui_aa_panel_rgb(const uint32_t* token, int role);
int mui_aa_panel_radius(const uint32_t* token, int font_px);
int mui_aa_panel_spacing(const uint32_t* token);

/* I667f (design 3.2, AASlider and AAProgress): track height and radius per tier, the colours by state, and the value <-> pixel maths.
 * No knob (owner decision 2026-10-29): only the TRACK track and the ACCENT fill are drawn, like the Mac desktop seek bar. */
typedef struct mui_aa_slider_t {
    int track_h, radius, min_w, height;
} mui_aa_slider_t;
void mui_aa_slider_metrics(int font_px, mui_aa_slider_t* out);
/* Track and filled part colours in a state (MUI_AA_NORMAL, _HOVER, _DISABLED): TRACK, ACCENT; disabled fill TEXT_DISABLED. Any out
 * pointer may be NULL. */
void mui_aa_slider_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* fill);
/* Filled pixels of a track of width w for value in [lo, hi] (clamped). */
int mui_aa_slider_fill_w(int w, long value, long lo, long hi);
/* Value for a pointer at box x, clamped to [lo, hi]. */
long mui_aa_slider_value_at(int w, int x, long lo, long hi);
/* Paint plan of an update draw (box-relative ops, painted in order). Op 0 is ALWAYS the erase of the full box to the parent
 * background (MADF_DRAWUPDATE does not repaint it, so without it every earlier fill stays visible), then the TRACK pill, then the
 * ACCENT fill pill when the fill is wider than 0. There is no knob op. Returns the op count (max MUI_AA_SLIDER_OPS). */
enum { MUI_AA_SLIDER_ERASE, MUI_AA_SLIDER_TRACK, MUI_AA_SLIDER_FILL };
typedef struct mui_aa_slider_op_t {
    int kind, x, y, w, h, radius;
    uint32_t rgb;
} mui_aa_slider_op_t;
#define MUI_AA_SLIDER_OPS 3
int mui_aa_slider_plan(const uint32_t* token, const mui_aa_slider_t* m, int state, int w, int h, long value, long lo, long hi, mui_aa_slider_op_t* out);
/* AAProgress: filled pixels of a bar of width w for value of max (clamped, max <= 0 gives 0). */
int mui_aa_progress_fill_w(int w, long value, long max);
/* Calls of the slider/progress maths above: the sentinel that the classes (and not a copy) run it. */
uint32_t mui_aa_slider_calls(void);

/* I667g (design 3.3, AACycle): face, text, chevron and popup colours by state, the chevron triangle, the minimum width, and the
 * Up/Down step with wrap. Tested on the host (mui-cycle-step). Chevron 8 px wide, gap 8 (tier S: 6 and 6). */
typedef struct mui_aa_cycle_t {
    int height, pad_x, radius, chev_w, chev_h, gap;
} mui_aa_cycle_t;
void mui_aa_cycle_metrics(int font_px, mui_aa_cycle_t* out);
/* Face SURFACE_RAISED (hover/pressed per state), value text TEXT (TEXT_DISABLED when disabled), chevron TEXT_MUTED (TEXT_DISABLED when
 * disabled), popup background SURFACE, popup row hover SURFACE_HOVER. Any out pointer may be NULL. */
void mui_aa_cycle_rgb(const uint32_t* token, int state, uint32_t* face, uint32_t* text, uint32_t* chevron, uint32_t* popup_bg, uint32_t* row_hover);
/* Minimum width: padding both sides, the widest entry, the gap and the chevron. */
int mui_aa_cycle_min_w(const mui_aa_cycle_t* m, int widest_text_w);
/* Row r (0 .. chev_h-1, top first) of the downward chevron triangle: x offset from the chevron left edge and the width. */
void mui_aa_cycle_chevron_row(const mui_aa_cycle_t* m, int r, int* x, int* w);
/* Active entry after a step of dir (-1 up, +1 down) in n entries, wrapping at both ends; n <= 0 gives 0. */
int mui_aa_cycle_step(int active, int n, int dir);
/* Whether v is a valid AAC_Active for n entries (a popup pick of "no row" is ignored). */
int mui_aa_cycle_valid(int v, int n);
/* Calls of the cycle maths above: the sentinel that the class (and not a copy) runs it. */
uint32_t mui_aa_cycle_calls(void);

/* I667h (design 3.4, AAField): the string gadget's flat rounded field. Face SURFACE, 1 px outline DIVIDER (FOCUS_RING while the field
 * is the active object), radius per tier (RADIUS 6, tier S 4), height as AAButton, inner pad 8 on every tier (more than the radius,
 * so the inner stock String never reaches the rounded corners). Tested on the host (mui-field-plan). */
typedef struct mui_aa_field_t {
    int height, pad_x, pad_y, radius, outline;
} mui_aa_field_t;
void mui_aa_field_metrics(int font_px, mui_aa_field_t* out);
/* Paint plan, painted in order, box-relative: op 0 the outline colour over the whole box (rounded), op 1 the face inset by the outline.
 * Returns the op count (MUI_AA_FIELD_OPS). */
#define MUI_AA_FIELD_OPS 2
/* Field face and outline tokens (I667h2): the face is one step lighter than the SURFACE panels the fields sit on, as the
 * desktop search field is lighter than its bar; the outline is TRACK so the edge reads on AGA too. */
#define MUI_AA_FIELD_FACE SEL_GUI_TOKEN_SURFACE_RAISED
#define MUI_AA_FIELD_LINE SEL_GUI_TOKEN_TRACK
int mui_aa_field_plan(const uint32_t* token, const mui_aa_field_t* m, int focus, mui_aa_check_op_t* out);
/* Field height for a content (text) height ch: the AAButton height, taller when the text needs it. */
int mui_aa_field_height(const mui_aa_field_t* m, int ch);
/* Calls of the field maths above: the sentinel that the class (and not a copy) runs it. */
uint32_t mui_aa_field_calls(void);

#ifdef __cplusplus
}
#endif
#endif
