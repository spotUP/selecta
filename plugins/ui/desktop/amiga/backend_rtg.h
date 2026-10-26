/* The `rtg` backend of the desktop UI (I287, M4 plan section 4.2): a Picasso96 screen of 16, 24 or 32 bits with one
 * borderless backdrop window for input. A present writes only the canvas dirty rectangles with p96WritePixelArray (the
 * library converts from the RGBA32 canvas, 0xAARRGGBB in big-endian memory, to the screen format). Input comes from
 * IDCMP and goes through sel_gui_input_map like the sdl backend. No Amiga type leaks out of this header.
 *
 * desktop_ui.c knows one backend interface, the desk_sdl_* names; this header maps them to desk_rtg_* so the same
 * call sites serve both. */
#ifndef SELECTA_DESKTOP_BACKEND_RTG_H
#define SELECTA_DESKTOP_BACKEND_RTG_H

#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_input.h>
#include <stdint.h>

typedef struct desk_rtg desk_rtg;

enum { DESK_RTG_NONE = 0, DESK_RTG_QUIT = 1, DESK_RTG_RESIZE = 2, DESK_RTG_ACTIVITY = 3 };

/* The canvas size when none is asked for: the size of the Workbench (default public) screen, so the UI opens full
 * screen (D-M4-3, I340). 1280x800 when intuition cannot be opened. */
void desk_rtg_default_size(int32_t* w, int32_t* h);
/* index8 != 0 opens an 8-bit CLUT screen (256 colours) and present expects an INDEX8 canvas; its palette is loaded on
 * the screen whenever it changed (I369). Otherwise a direct-colour screen and an RGBA32 canvas.
 * Opens the screen and window. w and h are the wanted size; 0 and 0 pick the largest RTG mode up to 1920 x 1080. NULL
 * (with a message on stderr) when Picasso96 or the mode is missing. */
desk_rtg* desk_rtg_open(int32_t w, int32_t h, int index8);
void desk_rtg_close(desk_rtg* b);
int32_t desk_rtg_width(const desk_rtg* b);
int32_t desk_rtg_height(const desk_rtg* b);
/* Bits per pixel of the screen: the sentinel that the screen is RTG (16 or more). */
int32_t desk_rtg_depth(const desk_rtg* b);
/* Writes canvas->dirty (call before sel_gui_present empties the list). `readback` is ignored: there is no readback. */
void desk_rtg_present(desk_rtg* b, const sel_gui_canvas* c, int readback);
uint32_t desk_rtg_upload_pixels(const desk_rtg* b);
uint32_t desk_rtg_presents(const desk_rtg* b);
/* Waits up to timeout_ms for window input, a timer or Ctrl-C (0 = poll) and queues every input message. Returns
 * DESK_RTG_QUIT on Ctrl-C, DESK_RTG_ACTIVITY when input was queued, else DESK_RTG_NONE. */
int desk_rtg_poll(desk_rtg* b, uint32_t timeout_ms);
/* Milliseconds from an arbitrary start, 20 ms steps (dos DateStamp). */
uint32_t desk_rtg_ticks(void);
/* Pops the oldest queued input; 0 when empty. The queue holds 64 events and drops the newest beyond that. */
int desk_rtg_next_input(desk_rtg* b, sel_gui_input* ev);

/* The sdl names desktop_ui.c uses, served by this backend. */
#define desk_sdl desk_rtg
#define DESK_SDL_NONE DESK_RTG_NONE
#define DESK_SDL_QUIT DESK_RTG_QUIT
#define DESK_SDL_RESIZE DESK_RTG_RESIZE
#define DESK_SDL_ACTIVITY DESK_RTG_ACTIVITY
#define desk_sdl_default_size desk_rtg_default_size
#define desk_sdl_close desk_rtg_close
#define desk_sdl_headless(b) ((void)(b), 0)
#define desk_sdl_scale(b) ((void)(b), 1)
#define desk_sdl_resize(b, w, h) ((void)(b), (w) == desk_rtg_width(b) && (h) == desk_rtg_height(b))
#define desk_sdl_present desk_rtg_present
#define desk_sdl_present_hash(b) ((void)(b), 0u)
#define desk_sdl_upload_pixels desk_rtg_upload_pixels
#define desk_sdl_presents desk_rtg_presents
#define desk_sdl_poll(b, ms, w, h) ((void)(w), (void)(h), desk_rtg_poll((b), (ms)))
#define desk_sdl_ticks desk_rtg_ticks
#define desk_sdl_open_folder_dialog(b) ((void)(b), 0)
#define desk_sdl_next_input desk_rtg_next_input
#define desk_sdl_now_ns() ((uint64_t)0u) /* wheel cost timing (I291) is SDL only */
#define desk_sdl_note_cost(b, t0) ((void)(b), (void)(t0))

#endif
