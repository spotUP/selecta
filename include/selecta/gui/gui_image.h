/* Artwork scaling, thumbnail packing and the artwork cache of the desktop UI (design doc 2.7, DUI-P2-4).
 * Integer code only. Decoding (stb_image) lives in the desktop plugin; this module takes decoded 0xAARRGGBB
 * words. */
#ifndef SELECTA_GUI_IMAGE_H
#define SELECTA_GUI_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Scales sw x sh words into dw x dh words with an integer box filter (nearest when enlarging). */
void sel_gui_image_scale(const uint32_t* src, int32_t sw, int32_t sh, uint32_t* dst, int32_t dw, int32_t dh);

/* Thumbnail blob written back to the library: "THM1", width and height as little-endian u16, then the pixel
 * words little-endian. Pack returns the byte count, 0 when `cap` is too small. Unpack returns 1 and fills
 * px/w/h, 0 on a malformed blob or when the pixels do not fit `cap_px`. */
#define SEL_GUI_THUMB_HEAD 8u
uint32_t sel_gui_thumb_pack(const uint32_t* px, int32_t w, int32_t h, uint8_t* out, uint32_t cap);
int sel_gui_thumb_unpack(const uint8_t* in, uint32_t len, uint32_t* px, uint32_t cap_px, int32_t* w, int32_t* h);

/* Artwork cache: a fixed pool of square slots per size token, allocated at open; the least recently used
 * slot of a size is reused. Not thread safe: the UI thread owns it. */
enum { SEL_GUI_ART_TILE = 0, SEL_GUI_ART_PANEL, SEL_GUI_ART_FLOW, SEL_GUI_ART_TINT, SEL_GUI_ART_SIZES };

typedef struct sel_gui_art_cache sel_gui_art_cache;

/* edge[i] = side in pixels of size token i, slots[i] = slot count. NULL when out of memory. */
sel_gui_art_cache* sel_gui_art_cache_open(const uint16_t edge[SEL_GUI_ART_SIZES], const uint8_t slots[SEL_GUI_ART_SIZES]);
void sel_gui_art_cache_close(sel_gui_art_cache* c);
/* Pixels of (size, key) or NULL on a miss; a hit becomes the most recently used. */
const uint32_t* sel_gui_art_cache_get(sel_gui_art_cache* c, int size, int64_t key);
/* Claims a slot for (size, key), evicting the least recently used one, and returns its edge x edge pixels. */
uint32_t* sel_gui_art_cache_put(sel_gui_art_cache* c, int size, int64_t key);
uint32_t sel_gui_art_cache_hits(const sel_gui_art_cache* c);
uint32_t sel_gui_art_cache_misses(const sel_gui_art_cache* c);

#ifdef __cplusplus
}
#endif

#endif /* SELECTA_GUI_IMAGE_H */
