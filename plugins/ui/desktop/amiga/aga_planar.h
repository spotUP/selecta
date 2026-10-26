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
/* Inverse: planes of a w x h screen back to a chunky buffer (frame dumps and tests). */
void desk_aga_p2c_ref(const uint8_t* planes, uint32_t bplsize, int32_t w, int32_t h, uint8_t* chunky);

/* Bytes of one 8-plane buffer (w * h) and of the double-buffered screen (two buffers), the chip RAM the backend
 * asks for at open (M4 plan 4.3: 640x512 = 640 KB, 640x256 = 320 KB). */
size_t desk_aga_buffer_bytes(int32_t w, int32_t h);
size_t desk_aga_chip_bytes(int32_t w, int32_t h);

/* Builds a graphics.library LoadRGB32 table for entries first .. first + n - 1 of 0xRRGGBB colours: word count and
 * first entry in the first longword, three left-justified 32-bit components per colour, a 0 terminator. out holds
 * n * 3 + 2 longwords. */
void desk_aga_rgb32_table(const uint32_t* rgb, uint32_t first, uint32_t n, uint32_t* out);

#endif
