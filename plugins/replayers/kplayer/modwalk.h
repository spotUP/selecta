/* ProTracker timing walker for the KPlayer adapter's native seek (I219g).
 * Walks the ORIGINAL module data (before KP_INIT converts it in place) with KPlayer v33's own row rules
 * (third_party/kplayer/kpl14.s: k_dskip, k_nextposition, k_positionjump, k_patternbreak, k_setspeed,
 * k_jumploop, k_patterndelay) and finds the row that is playing at a given time. Pure C, no Amiga headers,
 * so the host tests it. */
#ifndef SEL_KPLAYER_MODWALK_H
#define SEL_KPLAYER_MODWALK_H

#include <stddef.h>
#include <stdint.h>

typedef struct kp_walk_pos {
    uint8_t songpos; /* order index */
    uint8_t row;     /* pattern row, 0..63 */
    uint8_t speed;   /* ticks per row in effect when the row starts */
    uint8_t tempo;   /* BPM in effect when the row starts (CIA tick = 2.5 s / tempo) */
    uint32_t at_ms;  /* song time at the start of the row */
} kp_walk_pos;

/* Finds the row playing at `ms`: the last row that starts at or before it. Returns 1 and fills `out`, or 0 when
 * the module is malformed or the song ends (KPlayer's k_songover would count) before `ms`. The landing point is
 * the row start, so it lies at most one row (speed * 2.5 s / tempo) before `ms`. */
int kp_walk_find(const uint8_t* mod, size_t size, uint32_t ms, kp_walk_pos* out);

/* The song length: the time at which KPlayer's k_songover would count (the last order finishes, or a position jump
 * back onto itself). Returns 1 and sets *length_ms, or 0 when the module is malformed or never ends. */
int kp_walk_length(const uint8_t* mod, size_t size, uint32_t* length_ms);

/* I732: the pattern layout and cells of an unconverted M.K. / M!K! / FLT4 module, for the engine's tracker view
 * (RP_TRACKER_INFO / RP_TRACKER_CELLS). Same cell semantics as the nuade and openmpt adapters: note on libopenmpt's
 * scale (0 = none, C-0 = 1, so period 428 = 61), instrument 0 = none, effect and param as stored, no volume column. */
typedef struct kp_tracker_info {
    uint32_t orders, patterns, rows, channels;
} kp_tracker_info;

typedef struct kp_cell {
    uint8_t note, instrument, volume, effect, param;
} kp_cell;

/* Returns 1 and fills `out` when `order` is inside the order list, else 0. */
int kp_tracker_info_get(const uint8_t* mod, size_t size, uint32_t order, kp_tracker_info* out);

/* Writes the cells of `channel` for rows row_lo .. row_hi - 1 of the pattern at `order` into `out` (at most `cap`),
 * a row outside the pattern or the data as an empty cell. Returns how many, 0 when `order` or `channel` is out of
 * range. */
uint32_t kp_tracker_cells_get(const uint8_t* mod, size_t size, uint32_t order, uint32_t channel, uint32_t row_lo,
                              uint32_t row_hi, kp_cell* out, uint32_t cap);

#endif
