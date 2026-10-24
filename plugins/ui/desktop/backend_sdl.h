/* The `sdl` backend of the desktop UI (DUI-P2-7, plan section 2.4): a window with one streaming texture holding the
 * RGBA32 canvas. A present uploads only the canvas dirty rectangles (SDL_UpdateTexture per rectangle), renders the
 * texture and presents. No SDL type leaks out of this header, so desktop_ui.c stays buildable without SDL. */
#ifndef SELECTA_DESKTOP_BACKEND_SDL_H
#define SELECTA_DESKTOP_BACKEND_SDL_H

#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_input.h>
#include <stdint.h>

typedef struct desk_sdl desk_sdl;

enum { DESK_SDL_NONE = 0, DESK_SDL_QUIT = 1, DESK_SDL_RESIZE = 2, DESK_SDL_ACTIVITY = 3 };

/* Window of w x h pixels; NULL (with a message on stderr) when SDL or the video driver fails. */
desk_sdl* desk_sdl_open(int32_t w, int32_t h);
void desk_sdl_close(desk_sdl* b);
/* True for the offscreen and dummy drivers: no human can close or resize the window. */
int desk_sdl_headless(const desk_sdl* b);
/* The integer pixel scale 1 to 3 of the window: its pixel density rounded up (DUI-P2-8). */
int32_t desk_sdl_scale(const desk_sdl* b);
/* New canvas size: window and texture follow. 0 on failure. */
int desk_sdl_resize(desk_sdl* b, int32_t w, int32_t h);
/* Uploads canvas->dirty (call before sel_gui_present empties the list), renders and presents. With `readback` the
 * rendered frame is read back and hashed (sel_gui_hash) for desk_sdl_present_hash. */
void desk_sdl_present(desk_sdl* b, const sel_gui_canvas* c, int readback);
uint64_t desk_sdl_present_hash(const desk_sdl* b);
uint32_t desk_sdl_upload_pixels(const desk_sdl* b);
uint32_t desk_sdl_presents(const desk_sdl* b);
/* Waits up to timeout_ms for an event (0 = poll) and handles every queued one. Returns DESK_SDL_QUIT, DESK_SDL_RESIZE
 * (window pixel size changed by the user or by a scale change; *w and *h are the new size), DESK_SDL_ACTIVITY or
 * DESK_SDL_NONE. */
int desk_sdl_poll(desk_sdl* b, uint32_t timeout_ms, int32_t* w, int32_t* h);
uint32_t desk_sdl_ticks(void);
/* Live pointer and touchpad input (I242): desk_sdl_poll maps SDL mouse button, motion, wheel and pinch events (and the
 * back and forward buttons as a swipe) through sel_gui_input_map and queues the result. This pops the oldest queued
 * event into *ev; returns 0 when the queue is empty. The queue holds 64 events and drops the newest beyond that. */
int desk_sdl_next_input(desk_sdl* b, sel_gui_input* ev);

#endif
