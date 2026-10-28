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
enum { MUI_ICON_PX = 24, MUI_BTN_PAD_X = 10, MUI_BTN_PAD_Y = 6, MUI_BTN_ICON_GAP = 6 };

/* The baked bodies (generated at build time): count, edge in px, and one body per sel_gui_icon. */
extern const uint8_t* const mui_icon_bodies[];
extern const uint32_t mui_icon_body_count;
extern const uint32_t mui_icon_body_px;

/* Icon id (a sel_gui_icon value) for a button key: a transport or extra action id ("play", "pitch_up"), a sidebar door page
 * ("home") or a toolbar label ("Clear Queue"); the breadcrumb button uses "crumb". -1 when the key has no icon. Counts each
 * resolved lookup (mui_icon_resolved). */
int mui_icon_id(const char* key);
/* The Bodychunk body of an icon (mui_icon_body_px square, 2 planes, row interleaved), NULL for an id out of range. */
const uint8_t* mui_icon_body(int icon);
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
