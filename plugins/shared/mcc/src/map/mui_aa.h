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
/* TAB (I667j, AATabs): a tab caption on the PANEL, TEXT_MUTED idle, TEXT while its tab is active (state SELECTED); the face never changes
 * but for the hover token. */
enum { MUI_AA_ROLE_NORMAL, MUI_AA_ROLE_ACCENT, MUI_AA_ROLE_GHOST, MUI_AA_ROLE_TOGGLE, MUI_AA_ROLE_TAB };
/* Screen tiers by rows (section 3): S 640x256 and 320x256, M up to 600 rows, L up to 800, XL above. */
enum { MUI_AA_TIER_S, MUI_AA_TIER_M, MUI_AA_TIER_L, MUI_AA_TIER_XL };
/* Coverage ramp kinds (section 4): AGA four held token pens, RTG eight blended pens, TC none (ttengine blends). */
enum { MUI_AA_RAMP_AGA, MUI_AA_RAMP_RTG, MUI_AA_RAMP_TC };
#define MUI_AA_RAMP_MAX 8

typedef struct mui_aa_metrics_t {
    int font_px, pad_x, pad_y, icon_px, icon_gap, height, radius, check_px; /* check_px: AACheck box edge (16, tier S 12) */
    int aspect_q8; /* I949a: pixel height over width x 256; 256 square, 512 tall */
    int icon_h, check_h; /* I949e: icon and AACheck box height in ROWS: icon_px / check_px as a physical square, so half on a tall-pixel screen */
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
/* I972: bitmap font for the own screen, from a mui_aa_font_px size, so MUI's stock objects and the PLAIN text backend (no
 * ttengine or no Inter: text drawn in _font(obj), the screen font) grow with the tier like the antialiased text. Stock
 * Workbench 3.x helvetica (9 11 13 15 18 24) at the largest size not above font_px + 1. Returns 0 for font_px 0 (the 256-row
 * tier keeps the Workbench font, topaz 8) and fills nothing. */
typedef struct mui_aa_screen_font_t {
    const char* name; /* diskfont name, e.g. "helvetica.font" */
    int ysize;
} mui_aa_screen_font_t;
int mui_aa_screen_font(int font_px, mui_aa_screen_font_t* out);
int mui_aa_tier(int screen_rows);
/* Control heights and paddings of one tier table for a font size from mui_aa_font_px. */
void mui_aa_metrics(int font_px, mui_aa_metrics_t* out);
/* I949e (design 2026-11-01-I949 section 4, answer 4): the same on a screen of pixel aspect aspect_q8. Square pixels give mui_aa_metrics.
 * Tall pixels at the bitmap tier select the tight tier: pad_x 4, icon_gap 4, vertical sizes counted in rows (a row is aspect_q8/256 wide). */
void mui_aa_metrics_aspect(int font_px, int aspect_q8, mui_aa_metrics_t* out);
/* A length of px square pixels as rows on a screen of aspect_q8 (rounded, at least 1). The one place vertical sizes are scaled. */
int mui_aa_rows(int px, int aspect_q8);
/* The aspect of the open screen, set once by mui_screen_open and read by every class (none reads DisplayInfo). */
void mui_aa_set_screen_aspect(int aspect_q8);
int mui_aa_screen_aspect(void);
/* I949a: pixel aspect (height over width, x 256) from DisplayInfo.Resolution (ticks per pixel): hires PAL non-laced 22x44 gives 512,
 * laced and RTG about 1:1 give 256; clamped to 256..512. mui_pixel_aspect_calls counts the calls (sentinel for mui_screen_open). */
int mui_pixel_aspect_q8(int res_x, int res_y);
uint32_t mui_pixel_aspect_calls(void);

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
/* I949e2: group inner padding and spacing across (pad_x) and down (pad_y) for a screen of aspect_q8. 256 gives SPACING both ways. */
void mui_aa_panel_pad(const uint32_t* token, int aspect_q8, int* pad_x, int* pad_y);

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
/* I739: partial-draw plan of a value change (no erase): fill only when it grew, track then fill when it shrank, 0 ops when the fill width
 * equals prev_fw (prev_fw < 0: never drawn, so always paints both). mui_aa_slider_partial_count counts the non-empty plans. */
int mui_aa_slider_update_plan(const uint32_t* token, const mui_aa_slider_t* m, int state, int w, int h, int prev_fw, long value, long lo, long hi,
                              mui_aa_slider_op_t* out);
uint32_t mui_aa_slider_partial_count(void);
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
/* I667n: background of the inner String gadget in a focus state: the face of the plan (op 1). MUI 3.8 String fills its box with the
 * gadget's StringExtend background pen (Pens[1], ActivePens[1] when active), not with MUIA_Background, so the class sets that pen. */
uint32_t mui_aa_field_string_bg(const uint32_t* token, int focus);
/* I667n: the gadget MUIA_Gadget_Gadget hands back carries a StringInfo with the pens: a classic GTYP_STRGADGET, or the
 * GTYP_CUSTOMGADGET (strgclass) MUI 3.8 String really makes (rig 2: GadgetType 0005). */
int mui_aa_field_string_gadget(unsigned gadget_type);
/* Field height for a content (text) height ch: the AAButton height, taller when the text needs it. */
int mui_aa_field_height(const mui_aa_field_t* m, int ch);
/* Top edge of the text line (height ch) inside a field of height h: the room above and below the line is equal, so the group
 * centres the inner string with one flexible VSpace(0) on each side (I667h4). */
int mui_aa_field_text_top(const mui_aa_field_t* m, int h, int ch);
/* I733 (design 3.4): the placeholder of an empty, unfocused field. Returns 1 and the TEXT_MUTED colour when contents is empty (or NULL)
 * and the field is not the active object, else 0. Every 1 counts on mui_aa_field_placeholders(), the sentinel that the draw painted one. */
int mui_aa_field_placeholder(const uint32_t* token, const char* contents, int focus, uint32_t* rgb);
uint32_t mui_aa_field_placeholders(void);
/* Calls of the field maths above: the sentinel that the class (and not a copy) runs it. */
uint32_t mui_aa_field_calls(void);

/* I667i (design 3.5, AAScroller): the flat thumb-only scroll bar. Width 10 (tier S 6), thumb radius 4 (S 2), thumb minimum 20 high, track
 * SURFACE (the list face), thumb TRACK, SURFACE_PRESSED while hovered or dragged. No arrow buttons. Tested on the host (mui-scroller-geom). */
typedef struct mui_aa_scroller_t {
    int width, radius, min_thumb;
} mui_aa_scroller_t;
void mui_aa_scroller_metrics(int font_px, mui_aa_scroller_t* out);
void mui_aa_scroller_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* thumb);
/* Thumb top and height inside a track of height h for the first visible entry; both 0 and a 0 return when everything fits. The thumb
 * is at least min_thumb (at most h); first is clamped to [0, entries - visible]. */
int mui_aa_scroller_geom(const mui_aa_scroller_t* m, int h, long first, long visible, long entries, int* thumb_y, int* thumb_h);
/* First entry for a pointer at track y while the thumb is held grab pixels below its top; clamped to the ends. */
long mui_aa_scroller_first_at(const mui_aa_scroller_t* m, int h, int grab, int y, long visible, long entries);
uint32_t mui_aa_scroller_calls(void);

/* I667i (design 3.6, AAList; popup look 3.3): row model maths of the virtual list. Row height font + 8 (tier S 10), left pad 10, a vertical pad
 * of 4 (S 2) inside the face above the first and below the last row, column gap 8. Rows: SURFACE, hover SURFACE_HOVER, selected
 * SURFACE_PRESSED, playing title ACCENT. Tested on the host (mui-list-rows). */
typedef struct mui_aa_list_t {
    int row_h, pad_x, pad_y, gap, scroller_w, radius;
} mui_aa_list_t;
enum { MUI_AA_LIST_KEY_UP, MUI_AA_LIST_KEY_DOWN, MUI_AA_LIST_KEY_PAGEUP, MUI_AA_LIST_KEY_PAGEDOWN, MUI_AA_LIST_KEY_HOME, MUI_AA_LIST_KEY_END };
void mui_aa_list_metrics(int font_px, mui_aa_list_t* out);
/* Whole rows that fit a list face of height h (after the vertical pads). */
int mui_aa_list_visible(const mui_aa_list_t* m, int h);
long mui_aa_list_clamp_first(long first, long visible, long count);
/* Active row clamped into [0, count - 1]; -1 for an empty list. */
long mui_aa_list_clamp_active(long active, long count);
/* Active row after a key (MUI_AA_LIST_KEY_*), clamped (no wrap); an empty list gives -1, no active row starts at the first or last. */
long mui_aa_list_move(long active, long count, long visible, int key);
/* First row after scrolling just enough to show the active row. */
long mui_aa_list_scroll_to(long first, long visible, long count, long active);
/* Row under a pointer at list-relative y (-1 on the pads or past the last row). */
long mui_aa_list_row_at(const mui_aa_list_t* m, int y, long first, long count);
/* Row face (MUI_AA_NORMAL, _HOVER, _SELECTED) and text colour (ACCENT when playing). Any out pointer may be NULL. */
void mui_aa_list_row_rgb(const uint32_t* token, int row_state, int playing, uint32_t* face, uint32_t* text);
uint32_t mui_aa_list_calls(void);
/* Left edge (list-relative x) of the text of column col (0-based) when column k is w[k] wide: pad_x for column 0, then each column adds its
 * width, the gap on both sides of the divider and the 1 px divider. w may be NULL for column 0. Johan 2026-10-29: text sat 5 px from the frame
 * and from the dividers; the target is pad_x at the edge and gap (8) on each side of a divider, header row included. */
int mui_aa_list_col_x(const mui_aa_list_t* m, int col, const int* w);
/* MUIA_List_Format DELTA of a column followed by a divider (BAR): MUI centres the BAR in the delta, so gap on each side needs 2 * gap. */
int mui_aa_list_delta(const mui_aa_list_t* m);
/* I667i2: column widths of a multi-column list face of width total. Column k with weight[k] > 0 shares what is left after the fixed columns
 * (fixed[k] px each, weight 0) and the dividers (2 * gap + 1 each) and both pads, by weight; out[k] is its ink width (never negative). */
void mui_aa_list_col_widths(const mui_aa_list_t* m, int total, int n, const int* weight, const int* fixed, int* out);
/* Height of the title row (the column header): row_h when the list has one, else 0. */
int mui_aa_list_title_h(const mui_aa_list_t* m, int has_title);
/* Secondary column text (TEXT_MUTED, also the header text) and the header divider / column divider colour (DIVIDER). Any out pointer may be NULL. */
void mui_aa_list_header_rgb(const uint32_t* token, uint32_t* muted, uint32_t* divider);

/* I667j (design 3.9, AATabs): a row of tab captions with a 2 px ACCENT underline under the active one. Tested on the host (mui-tabs). */
#define MUI_AA_TABS_LINE_PX 2
/* Index after a click on tab i (out of range gives -1: no change) and after a key (Left 0x4F -1, Right 0x4E +1, clamped, no wrap; n <= 0 gives 0). */
int mui_aa_tabs_click(int i, int n);
int mui_aa_tabs_key(int code, int active, int n);
/* Underline rectangle for the active tab whose box is (x, y, w, h): full tab width, MUI_AA_TABS_LINE_PX high (tier S, font_px 0: 1), at the bottom edge. */
void mui_aa_tabs_line(int font_px, int x, int y, int w, int h, int* lx, int* ly, int* lw, int* lh);
/* Underline ACCENT; caption colour idle TEXT_MUTED, active TEXT. Any out pointer may be NULL. */
void mui_aa_tabs_rgb(const uint32_t* token, uint32_t* line, uint32_t* idle_text, uint32_t* active_text);
uint32_t mui_aa_tabs_calls(void);

/* I667l (design 3.10, AABalance): the pane divider is the panel gap itself. Box width = SPACING token (tier S, font_px 0: half), one pixel
 * DIVIDER line centred in it, inset by the same gap top and bottom. Box colour BACKGROUND. Tested on the host (mui-balance-geom). */
int mui_aa_balance_width(const uint32_t* token, int font_px);
/* Line rectangle inside a box of box_w x box_h (origin 0,0); inset is the gap of that tier; a box shorter than two insets gets inset 0 (clamp). */
void mui_aa_balance_line(const uint32_t* token, int font_px, int box_w, int box_h, int* lx, int* ly, int* lw, int* lh);
/* Box colour BACKGROUND, line colour DIVIDER. Any out pointer may be NULL. */
void mui_aa_balance_rgb(const uint32_t* token, uint32_t* box, uint32_t* line);
uint32_t mui_aa_balance_calls(void);

/* I731 (plan 2026-10-31-I731-mui-flat-tooltips.md section 2.1): the flat tooltip. Pure plan, placement and hover state; the Amiga module
 * (../mui_aatip.c) draws and times. Tested on the host (mui-tooltip-flat). */
enum { MUI_AA_TIP_ABOVE, MUI_AA_TIP_BELOW, MUI_AA_TIP_RIGHT, MUI_AA_TIP_LEFT };
typedef struct mui_aa_tip_t {
    int w, h, pad_x, pad_y, radius, gap;
    uint32_t face, text, corner; /* 0xRRGGBB */
} mui_aa_tip_t;
/* Box size and colours: face = SURFACE_PRESSED, text = TEXT, corner = SURFACE (the panel colour the wedges outside the rounded box are
 * painted with), radius = RADIUS_SMALL clamped to h / 2, pad_x = SPACING, pad_y = SPACING / 2 (tier S: font_px <= 0 -> pad_x 6, pad_y 3,
 * radius 4), gap 4 (tier S 2). w = text_w + 2 * pad_x, h = text_h + 2 * pad_y. */
void mui_aa_tip_plan(const uint32_t* token, int font_px, int text_w, int text_h, mui_aa_tip_t* out);
/* Screen position of a w x h box next to the anchor rect (screen coordinates), side pref first; ABOVE that does not fit flips BELOW and back,
 * RIGHT flips LEFT and back; the box is then clamped inside 0..scr_w-w, 0..scr_h-h. Returns the side used. */
int mui_aa_tip_place(int ax, int ay, int aw, int ah, int w, int h, int scr_w, int scr_h, int gap, int pref, int* x, int* y);
/* The hover state machine (plan section 4). */
enum { MUI_AA_TIP_IDLE, MUI_AA_TIP_PENDING, MUI_AA_TIP_SHOWN, MUI_AA_TIP_MUTED };
enum { MUI_AA_TIP_EV_ENTER, MUI_AA_TIP_EV_LEAVE, MUI_AA_TIP_EV_TIMER, MUI_AA_TIP_EV_PRESS, MUI_AA_TIP_EV_KEY, MUI_AA_TIP_EV_INACTIVE, MUI_AA_TIP_EV_GONE };
enum { MUI_AA_TIP_DO_NONE = 0, MUI_AA_TIP_DO_ARM = 1, MUI_AA_TIP_DO_DISARM = 2, MUI_AA_TIP_DO_SHOW = 4, MUI_AA_TIP_DO_HIDE = 8 };
int mui_aa_tip_step(int state, int ev, int* actions); /* returns the next state, ORs the actions into *actions */
#define MUI_AA_TIP_DELAY_MS 600 /* desktop TIP_DELAY_MS */
uint32_t mui_aa_tip_calls(void); /* sentinel: mui_aa_tip_plan calls */

/* I973b: an icon button's size request, pure. fixed_w / fixed_h in px (0 = none). The deltas are added to the Area's own insets.
 * A fixed side has min = def = max, so no weight can stretch the square; a labelled button keeps the unbounded width. */
typedef struct mui_btn_box {
    uint32_t min_w, def_w, min_h, def_h;
    uint32_t w_fixed; /* 1: MaxWidth = MinWidth, else unbounded */
    uint32_t h_fixed; /* always 1 today: a button never grows taller than its contents */
} mui_btn_box_t;
void mui_aabtn_minmax(uint32_t content_w, uint32_t content_h, uint32_t fixed_w, uint32_t fixed_h, uint32_t toggle, mui_btn_box_t* out);

#ifdef __cplusplus
}
#endif
#endif
