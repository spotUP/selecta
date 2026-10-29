/* Album artwork of the desktop UI (design doc 2.7, DUI-P2-4): a worker thread decodes covers (stb_image) into the
 * artwork cache and writes thumbnails back to the library through the shell's sel_ui_art table. */
#ifndef DESKTOP_ART_H
#define DESKTOP_ART_H

#include <selecta/gui/gui_image.h>
#include <selecta/ui.h>

#include <stdint.h>

#define DESK_ART_TILE_EDGE 144 /* the tile artwork size token, pixels */

typedef struct desk_art desk_art;

/* NULL when `src` is NULL (no library) or out of memory. */
desk_art* art_open(const sel_ui_art* src);
void art_close(desk_art* a);
/* Resolves the albums of the songs at the queue paths `titles[0..n)` that are not resolved yet and loads their tile artwork on the worker
 * thread: from the thumb row when the library has one (no decode), else by decoding the cover and writing the
 * thumb row back. Returns when the worker is done, so the frame that follows is deterministic; the interactive
 * backend will poll instead (DUI-P2-7). */
void art_load(desk_art* a, const char* const* titles, uint32_t n);
/* The tile pixels (DESK_ART_TILE_EDGE squared) of the album of the song at queue path `title`, NULL when it has no artwork. */
const uint32_t* art_tile(desk_art* a, const char* title);
/* I678e: the dominant colour (0xAARRGGBB) of the cover of the album of the song at `title`; 0 when it has no artwork. Computed once per art
 * entry and cached; art_dominants counts the computations. */
int art_dominant(desk_art* a, const char* title, uint32_t* argb);
uint32_t art_dominants(const desk_art* a);
/* Covers decoded since open (STAT_ART_DECODES). */
uint32_t art_decodes(const desk_art* a);

/* stb_impl.c: decodes a PNG or JPEG into malloc'd 0xAARRGGBB words; NULL on failure. */
uint32_t* desk_image_decode(const uint8_t* data, uint32_t len, int32_t* w, int32_t* h);

/* posix/art_thread.c: runs fn(ctx) on a new thread. Returns a handle for art_thread_join, or NULL when no thread
 * could start (fn has run inline then). */
void* art_thread_start(void (*fn)(void*), void* ctx);
void art_thread_join(void* handle);

#endif /* DESKTOP_ART_H */
