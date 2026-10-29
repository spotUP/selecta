/* I651: icons on the MUI buttons. The shapes are the desktop UI's Lucide icons (include/selecta/gui/gui_icons.h, rasterised
 * by tools/gui_iconbake.c at build time) written as MUI 3.8 Bodychunk bodies (2 bitplanes, pen 0 transparent); no second
 * icon family, nothing drawn per frame (MUI blits the Bodychunk bitmap it remaps once at layout). The palette comes from
 * the theme tokens (mui_theme.c colours) so icons follow the Selecta look. Pure C99. */
#ifndef SELECTA_MUI_ICONS_H
#define SELECTA_MUI_ICONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Button geometry in pixels: the icon edge and the MUI inner padding around the button contents. The label sits right of
 * the icon; the old text-only button had MUI's default 1 px padding. */
enum { MUI_BTN_PAD_X = 10, MUI_BTN_PAD_Y = 6, MUI_BTN_ICON_GAP = 6 };
/* I656: an icon-only button is a group whose only child is a fixed-size image, so its maximum width is fixed and bounds the
 * row, then the root group, then MUIV_Window_Width_Screen (the window opened 446 px wide). Such a button gets this many
 * HSpace(0) children around the image: they have no minimum but are stretchable, so the button grows like a text button
 * and the icon stays centered. */
static inline unsigned mui_button_spacers(int icon_only, int has_icon) {
    return icon_only && has_icon ? 2u : 0u;
}
/* Width model of a button (I656): MUI frame 2 px a side, the 8 px topaz label font, the volume slider's minimum. The dense rows
 * (toolbar, extras) are icon-only with the label as MUIA_ShortHelp so every row fits the 640 px smallest screen (plan P6-2);
 * doors, crumbs and the transport row keep icon and label. */
enum { MUI_BTN_FRAME = 4, MUI_FONT_W = 8, MUI_SLIDER_MIN = 64, MUI_MIN_SCREEN_W = 640, MUI_TOOL_ICON_ONLY = 1, MUI_EXTRA_ICON_ONLY = 1 };
/* Minimum width of one button: padding, frame, the icon and, unless icon_only, the gap and the label text. */
uint32_t mui_button_width(const char* label, int icon_only);

/* The baked bodies (generated at build time): count, edge in px, and one body per sel_gui_icon. */
/* One set per size (mui_icon_sizes, ascending), one body per sel_gui_icon in each set. */
extern const uint8_t* const* const mui_icon_sets[];
extern const uint32_t mui_icon_size_count;
extern const uint32_t mui_icon_sizes[];
extern const uint32_t mui_icon_body_count;

/* Icon id (a sel_gui_icon value) for a button key: a transport or extra action id ("play", "pitch_up"), a sidebar door page
 * ("home") or a toolbar label ("Clear Queue"); the breadcrumb button uses "crumb". -1 when the key has no icon. Counts each
 * resolved lookup (mui_icon_resolved). */
int mui_icon_id(const char* key);
/* The Bodychunk body of an icon at px edge (px square, 2 planes, row interleaved), NULL for an id out of range. */
const uint8_t* mui_icon_body(int icon, int px);
/* Icon edge the width and height models assume: the tier M size (mui_aa_metrics), the tier of the laced 640x512 screen. */
int mui_model_icon_px(void);
/* I667m sentinel: mui_icon_body lookups that found a body of this size since start. */
uint32_t mui_icon_body_lookups(int px);
/* Number of successful mui_icon_id lookups since start (sentinel for the build path test). */
uint32_t mui_icon_resolved(void);

/* MUIA_Bitmap_SourceColors for 4 pens: 12 ULONG values (RGB32, each channel 0xRRRRRRRR). themed: pen 0 and the fade start
 * at the button face (SURFACE_RAISED), pen 3 is the TEXT token; unthemed: MUI's grey face 0xAAAAAA and black. Pens 1 and 2
 * are one third and two thirds of the way. tokens is the SEL_GUI_TOKEN_COUNT array of a txt_theme. */
void mui_icon_palette(const uint32_t* tokens, int themed, uint32_t out[12]);

#ifdef __cplusplus
}
#endif
#endif
