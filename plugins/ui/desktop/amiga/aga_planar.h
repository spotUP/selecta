/* Pure helpers of the `aga` backend (I439, M4-P6-1, design thoughts/shared/plans/2026-10-26-aga-c2p-backend.md):
 * dirty rectangles to full-width row bands, a C reference 1x1 8-bitplane chunky-to-planar conversion and its
 * inverse, the chip RAM budget and the LoadRGB32 palette table. No Amiga headers: the host builds this file for the
 * dui-aga-planar unit test, the Amiga build links it into backend_aga.c.
 *
 * Bitplane layout is the one Kalms' normal/ c2p routines write: 8 contiguous planes, plane p starts at
 * planes + p * bplsize, a plane row is w / 8 bytes, the leftmost pixel of a byte is its bit 7, and bit p of a chunky
 * pixel goes to plane p. */
#ifndef SELECTA_DESKTOP_AGA_PLANAR_H
#define SELECTA_DESKTOP_AGA_PLANAR_H

#include <selecta/gui/gui_draw.h>
#include <stddef.h>
#include <stdint.h>

#define DESK_AGA_DEPTH 8
#define DESK_AGA_BANDS_MAX 16

typedef struct desk_aga_band {
    int32_t y, h;
} desk_aga_band;

/* Turns n rectangles into sorted, disjoint row bands clipped to rows [0, h). Bands whose gap is at most `gap` rows
 * are joined (a c2p call has a fixed set-up cost, so two nearby bands cost more than one taller band). At most max
 * bands; beyond that the closest pair is joined until it fits. Returns the band count. */
uint32_t desk_aga_bands(const sel_gui_rect* r, uint32_t n, int32_t h, int32_t gap, desk_aga_band* out, uint32_t max);
/* Merges b (nb bands) into a (na bands, room for max) with the same rules; returns the new count of a. Used to add
 * the previous frame's bands when writing the back buffer of a double-buffered screen. */
uint32_t desk_aga_bands_union(desk_aga_band* a, uint32_t na, const desk_aga_band* b, uint32_t nb, int32_t gap,
                              uint32_t max);
/* Rows covered by the bands. */
uint32_t desk_aga_band_rows(const desk_aga_band* b, uint32_t n);

/* Reference conversion of rows [y, y + rows) of a chunky buffer w pixels wide (row stride w) into the planes. w is a
 * multiple of 8. The planes outside those rows are not touched. */
void desk_aga_c2p_ref(const uint8_t* chunky, int32_t w, int32_t y, int32_t rows, uint8_t* planes, uint32_t bplsize);
/* I574: the c2p for a CPU, from exec's AttnFlags (AFF_68020 bit 1, AFF_68030 bit 2, AFF_68040 bit 3, AFF_68060 bit 7):
 * Kalms' 040 routine on a 68040/060, his 030 routine on a 68020/030 (DESK_AGA_C2P_* of backend_aga.h). The C reference
 * is only for a CPU below the 68020; on a 68020 it took 16 s per 640x256 frame, which froze the pointer. */
#define DESK_AGA_C2P_030_MAX_BPLSIZE 16384u
/* I580: the 030 routine only while one plane (bplsize bytes) fits its 16 KB limit; a larger screen (640x256 is 20480
 * bytes a plane) gets the 040 routine on a 68020/030 too. */
uint32_t desk_aga_c2p_pick(uint32_t attn_flags, uint32_t bplsize);
/* Inverse: planes of a w x h screen back to a chunky buffer (frame dumps and tests). */
void desk_aga_p2c_ref(const uint8_t* planes, uint32_t bplsize, int32_t w, int32_t h, uint8_t* chunky);

/* Bytes of one 8-plane buffer (w * h) and of the double-buffered screen (two buffers), the chip RAM the backend
 * asks for at open (M4 plan 4.3: 640x512 = 640 KB, 640x256 = 320 KB). */
size_t desk_aga_buffer_bytes(int32_t w, int32_t h);
size_t desk_aga_chip_bytes(int32_t w, int32_t h);

/* Builds a graphics.library LoadRGB32 table for entries first .. first + n - 1 of 0xRRGGBB colours: word count and
 * first entry in the first longword, three left-justified 32-bit components per colour, a 0 terminator. out holds
 * n * 3 + 2 longwords. */
/* I441: the four copper register writes that set palette pen `pen` (0-255) to rgb (0xRRGGBB) at 24-bit depth on AGA:
 * BPLCON3 with the pen's bank and LOCT clear, the high nibbles, BPLCON3 with LOCT set, the low nibbles. out[i][0] is the
 * custom chip register offset, out[i][1] the value. */
void desk_aga_copper_regs(uint32_t pen, uint32_t rgb, uint32_t out[4][2]);
void desk_aga_rgb32_table(const uint32_t* rgb, uint32_t first, uint32_t n, uint32_t* out);

/* I441: 1 when a graphics.library display mode ID belongs to the native chipset (the monitors graphics knows: default,
 * NTSC, PAL, VGA, A2024, Euro, Super72, DblNTSC, DblPAL, all below 0x00100000 in the monitor part), 0 for an RTG mode
 * (CyberGraphX 0x40xxxxxx, Picasso96 0x50xxxxxx and any other board's high monitor IDs) and for INVALID_ID. The AGA
 * screen opens on a native mode only: an RTG screen shows its planar bitmap black and has no copper. */
int desk_aga_mode_is_native(uint32_t mode_id);

/* The AGA screen is Selecta's own, whatever Workbench runs: 640x256 PAL HIRES (640x200 on NTSC) by default, or
 * 320x256 PAL LORES (320x200 NTSC) when asked (Johan 2026-10-27; an 800x600 or interlaced screen is never opened).
 * Fills w and h when either is <= 0. Returns 1 for HIRES, 0 for LORES, -1 when the asked size is neither. */
int desk_aga_pick_size(int32_t* w, int32_t* h, int ntsc, int32_t max_w, int32_t max_h);

/* I566: BPLCON3 of every copper write Selecta makes: SPRES bits, BRDRBLNK (0x20) so the border outside the bitplane
 * window is black. Without it the border shows COLOR00, which the desktop palette holds as the accent blue. */
#define DESK_AGA_BPLCON3 0x0C20u

/* I566: the screen fills the user's Overscan preferences area, as Workbench does: QueryOverscan type OSCAN_STANDARD (2),
 * never OSCAN_MAX (3), which reaches beyond what a monitor (or the FS-UAE window) shows and cut the left side off. */
#define DESK_AGA_OSCAN_TYPE 2

/* I566: the screen fills the visible area. max_w x max_h is what QueryOverscan(DESK_AGA_OSCAN_TYPE) reports for the HIRES
 * mode; the canvas takes it rounded to what the c2p needs (width a multiple of 32, height a multiple of 8). Untuned
 * preferences report 640 x 256 (640 x 200 NTSC) or less, which gets the standard size and the normal screen. */
void desk_aga_fit_size(int32_t max_w, int32_t max_h, int ntsc, int32_t* w, int32_t* h);

/* I566: the setting ui.desktop.overscan. Off: the standard 640 x lines screen (the overscan area is reported as 0 x 0, so
 * desk_aga_pick_size keeps the standard size); on: the QueryOverscan area stays and the screen fills it. */
void desk_aga_overscan_area(int overscan, int32_t* max_w, int32_t* max_h);

/* I566: the graphics.library VideoControl tag list that blanks the border. VTAG_BORDERBLANK_SET makes graphics.library
 * set BRDRBLNK and ECSENA (BPLCON0 bit 0, without which the chipset ignores BRDRBLNK). Returns the tag count incl. TAG_DONE. */
#define DESK_AGA_VTAG_BORDERBLANK_SET 0x80000005u
int desk_aga_border_blank_tags(uint32_t tags[4]);

/* The DClip rectangle (left, top, right, bottom, inclusive) of a w x h screen centred inside the OSCAN_STANDARD rectangle. */
void desk_aga_fit_clip(const int32_t max_rect[4], int32_t w, int32_t h, int32_t out[4]);

#endif
