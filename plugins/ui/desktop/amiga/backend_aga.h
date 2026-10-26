/* The `aga` backend of the desktop UI (I439, M4-P6-1, design thoughts/shared/plans/2026-10-26-aga-c2p-backend.md):
 * a system-friendly Intuition screen of 8 bitplanes in chip RAM, double buffered with AllocScreenBuffer /
 * ChangeScreenBuffer. The UI draws an INDEX8 canvas in fast RAM; a present converts only the rows of the canvas dirty
 * rectangles (plus the rows the other buffer still lacks) with Kalms' c2p1x1_8_c5_040 on 68040/68060, else with the
 * C reference of aga_planar.c. The canvas palette is loaded with LoadRGB32 when it changed. The OS, AHI and Wasabi
 * keep running: no LoadView(NULL), no Forbid, no input handler.
 *
 * Same interface as backend_rtg.h (desktop_ui.c knows only the desk_sdl_* names). No Amiga type leaks out. */
#ifndef SELECTA_DESKTOP_BACKEND_AGA_H
#define SELECTA_DESKTOP_BACKEND_AGA_H

#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_input.h>
#include <stddef.h>
#include <stdint.h>

typedef struct desk_aga desk_aga;

enum { DESK_AGA_NONE = 0, DESK_AGA_QUIT = 1, DESK_AGA_RESIZE = 2, DESK_AGA_ACTIVITY = 3 };
enum { DESK_AGA_C2P_REF = 0, DESK_AGA_C2P_KALMS_040 = 1 };

/* The canvas size when none is asked for: the Workbench size when Workbench runs on a native mode that allows 8
 * bitplanes, else 640x256 (PAL) or 640x200 (NTSC) (M4-P6-8). */
void desk_aga_default_size(int32_t* w, int32_t* h);
/* Opens the screen, the two chip RAM buffers and the backdrop window. index8 must be non-zero (an AGA screen shows
 * an INDEX8 canvas only); w must be a multiple of 32. NULL with a message on stdout when the chip RAM free is below
 * desk_aga_chip_bytes(w, h), the mode is missing or a library does not open. */
desk_aga* desk_aga_open(int32_t w, int32_t h, int index8);
void desk_aga_close(desk_aga* b);
int32_t desk_aga_width(const desk_aga* b);
int32_t desk_aga_height(const desk_aga* b);
/* Bitplanes of the screen (8): the sentinel that the screen is AGA planar. */
int32_t desk_aga_depth(const desk_aga* b);
/* Converts the dirty rows of the INDEX8 canvas into the back buffer and shows it (call before sel_gui_present empties
 * the dirty list). */
void desk_aga_present(desk_aga* b, const sel_gui_canvas* c);
/* Pixels converted (rows x width), presents, c2p calls, the c2p in use, chip RAM bytes held by the two buffers. */
uint32_t desk_aga_upload_pixels(const desk_aga* b);
uint32_t desk_aga_presents(const desk_aga* b);
uint32_t desk_aga_c2p_calls(const desk_aga* b);
uint32_t desk_aga_c2p_kind(const desk_aga* b);
size_t desk_aga_chip_held(const desk_aga* b);
/* Waits up to timeout_ms for window input, a timer or Ctrl-C (0 = poll). Returns DESK_AGA_QUIT on Ctrl-C,
 * DESK_AGA_ACTIVITY when input was queued, else DESK_AGA_NONE. The IDCMP translation is the shared amiga/input.c. */
int desk_aga_poll(desk_aga* b, uint32_t timeout_ms);
uint32_t desk_aga_ticks(void);
int desk_aga_next_input(desk_aga* b, sel_gui_input* ev);

/* The sdl names desktop_ui.c uses, served by this backend. */
#define desk_sdl desk_aga
#define DESK_SDL_NONE DESK_AGA_NONE
#define DESK_SDL_QUIT DESK_AGA_QUIT
#define DESK_SDL_RESIZE DESK_AGA_RESIZE
#define DESK_SDL_ACTIVITY DESK_AGA_ACTIVITY
#define desk_sdl_default_size desk_aga_default_size
#define desk_sdl_close desk_aga_close
#define desk_sdl_headless(b) ((void)(b), 0)
#define desk_sdl_scale(b) ((void)(b), 1)
#define desk_sdl_resize(b, w, h) ((void)(b), (w) == desk_aga_width(b) && (h) == desk_aga_height(b))
#define desk_sdl_present desk_aga_present
#define desk_sdl_present_hash(b) ((void)(b), 0u)
#define desk_sdl_readbacks(b) ((void)(b), 0u)
#define desk_sdl_raises(b) ((void)(b), 0u) /* a new Amiga screen opens in front; nothing raises it (I501) */
#define desk_sdl_upload_pixels desk_aga_upload_pixels
#define desk_sdl_presents desk_aga_presents
#define desk_sdl_poll(b, ms, w, h) ((void)(w), (void)(h), desk_aga_poll((b), (ms)))
#define desk_sdl_ticks desk_aga_ticks
#define desk_sdl_open_folder_dialog(b) ((void)(b), 0)
#define desk_sdl_next_input desk_aga_next_input
#define desk_sdl_now_ns() ((uint64_t)0u)
#define desk_sdl_note_cost(b, t0) ((void)(b), (void)(t0))

#endif
