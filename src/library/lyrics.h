/* Synced and plain lyrics (F19, I193, design doc section 9): the LRC parser, the current-line lookup and the path of
 * the `.lrc` next to a song. Pure functions on caller-owned fixed buffers, no allocation, so the 68k build parses
 * with the same code. Where the text comes from (a file next to the song, the tracker's song message, the DEViLBOX
 * endpoint) is the engine's business; this module only reads text. */
#ifndef SELECTA_LIBRARY_LYRICS_H
#define SELECTA_LIBRARY_LYRICS_H

#include <stddef.h>
#include <stdint.h>

#define SEL_LRC_MAX_LINES 512u  /* timed lines kept; later ones are dropped */
#define SEL_LRC_TEXT_MAX 16384u /* bytes of line text kept (NUL separated) */

typedef struct sel_lrc {
    uint32_t count;                  /* timed lines, sorted by time */
    uint32_t ms[SEL_LRC_MAX_LINES];  /* start of line i, milliseconds */
    uint16_t off[SEL_LRC_MAX_LINES]; /* offset of line i's text in `text` */
    uint16_t used;                   /* bytes of `text` in use */
    char text[SEL_LRC_TEXT_MAX];
} sel_lrc;

/* Parses LRC text: `[mm:ss.xx]text` lines (a line may carry several stamps; `[mm:ss]`, `[mm:ss.x]`, `[mm:ss.xxx]` and
 * `[mm:ss:xx]` are accepted), the `[offset:+/-ms]` tag (positive: lyrics appear that much earlier), other `[tag:value]`
 * header lines are ignored. Returns the number of timed lines, 0 when the text carries no timestamp (then it is
 * not LRC and *out is empty). */
uint32_t sel_lrc_parse(sel_lrc* out, const char* src, size_t len);

/* The text of timed line `i` (empty string for an instrumental gap line), NULL when i is out of range. */
const char* sel_lrc_text(const sel_lrc* l, uint32_t i);

/* Index of the line being sung at `ms`: the last line whose start is <= ms; -1 before the first line or when empty. */
int32_t sel_lrc_index_at(const sel_lrc* l, uint32_t ms);

/* Writes the `.lrc` path that belongs to `song` (the extension replaced; a name without one gets `.lrc` appended).
 * 1 on success, 0 when it does not fit. */
int sel_lrc_path(const char* song, char* out, size_t cap);

#endif
