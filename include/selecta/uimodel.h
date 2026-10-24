/* UI model shared by every UI that draws regions (desktop, custom Amiga UI): the frame strings, transport flags,
 * progress in Q16 and one dirty bit per region, built from sel_engine_state() once per pump (plan
 * 2026-10-24-desktop-ui.md DUI-P1-4, M4 plan section 2). Plain C99: no float, no allocation, fixed buffers. */
#ifndef SELECTA_UIMODEL_H
#define SELECTA_UIMODEL_H

#include <selecta/engine.h>

SEL_EXTERN_C_BEGIN

/* Regions of a frame. A widget redraws only when its region's bit is set. */
enum {
    SEL_UIM_DIRTY_SONG = 1u,      /* title, artist, format */
    SEL_UIM_DIRTY_QUEUE = 2u,     /* entry count, playing entry */
    SEL_UIM_DIRTY_TRANSPORT = 4u, /* play state, previous and next availability */
    SEL_UIM_DIRTY_PROGRESS = 8u,  /* elapsed and remaining clock, progress bar */
    SEL_UIM_DIRTY_VOLUME = 16u,
    SEL_UIM_DIRTY_STATUS = 32u    /* status line */
};
#define SEL_UIM_REGIONS 6u
#define SEL_UIM_DIRTY_ALL ((1u << SEL_UIM_REGIONS) - 1u)

typedef struct sel_uim_frame {
    char title[96];
    char artist[64];
    char format[24];
    char clock[16];     /* elapsed, "m:ss" */
    char remaining[16]; /* "-m:ss", empty when the duration is unknown */
    char status[96];
    sel_play_state play;
    uint32_t can_prev, can_next;
    uint32_t progress_q16; /* 0 .. 65536, 0 when the duration is unknown */
    uint32_t queue_count, queue_playing;
    int32_t volume;
} sel_uim_frame;

/* Frame layouts, the values of the pref ui.desktop.mode (full, window, mini). */
enum { SEL_UIM_LAYOUT_FULL = 0, SEL_UIM_LAYOUT_WINDOW = 1, SEL_UIM_LAYOUT_MINI = 2 };

typedef struct sel_uim {
    sel_uim_frame frame;
    uint32_t layout;      /* SEL_UIM_LAYOUT_* */
    uint32_t dirty;       /* regions changed since sel_uim_take_dirty */
    uint32_t last_taken;  /* regions of the latest frame that had any to redraw */
    uint32_t updates;     /* sel_uim_update calls */
} sel_uim;

/* Empty model with every region dirty (the first frame draws everything). */
void sel_uim_init(sel_uim* uim);
/* Rebuild the frame from the engine snapshot; returns the regions whose content changed in this update. */
uint32_t sel_uim_update(sel_uim* uim, const sel_state* st);
/* Select the layout by its pref value; returns 0 for an unknown name. A layout change dirties every region. */
int sel_uim_set_layout(sel_uim* uim, const char* mode);
/* The regions to redraw; clears them. */
uint32_t sel_uim_take_dirty(sel_uim* uim);

SEL_EXTERN_C_END

#endif /* SELECTA_UIMODEL_H */
