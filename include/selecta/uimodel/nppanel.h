/* I1222f: the Now Playing panel content (src/uimodel/nppanel.c), taken out of the desktop's draw_nowplaying so the text UI draws the same
 * lines. Content only, no geometry and no drawing: the context heading, title, artist, like and rating, the catalogue pairs (up to 6),
 * the song note, the lyric lines or tracker message, and Up Next (up to 3). Pure C99. Design: thoughts/shared/research/2026-11-09-text-frame-design.md
 * section 4.4. */
#ifndef SELECTA_UIMODEL_NPPANEL_H
#define SELECTA_UIMODEL_NPPANEL_H
#include <selecta/engine.h>
#include <selecta/uimodel.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SEL_UIM_NP_PAIRS_MAX 6u
#define SEL_UIM_NP_UPNEXT_MAX 3u
/* The most lines sel_uim_np_lines can write. */
#define SEL_UIM_NP_LINES_MAX 20u

typedef enum {
    SEL_UIM_NP_CONTEXT,     /* val: the context the song plays from (playlist, else release, else "Now Playing") */
    SEL_UIM_NP_TITLE,       /* val: the title */
    SEL_UIM_NP_ARTIST,      /* val: the artist; absent when the song has none */
    SEL_UIM_NP_LIKE,        /* liked (0/1) and rating (0..5) */
    SEL_UIM_NP_PAIR,        /* key: label, val: value cut to 255 bytes, ref: the whole value (catalogue rows of the open song, in panel order) */
    SEL_UIM_NP_NOTE,        /* val: the note under the pairs; key: the action label ("Get File...") or empty */
    SEL_UIM_NP_LYRIC,       /* key: "previous", "current" or "next"; val: the synced lyric line (may be empty) */
    SEL_UIM_NP_MESSAGE,     /* ref: the tracker song message (several lines, '\n' apart), owned by the state */
    SEL_UIM_NP_UPNEXT_HDR,  /* val: "Next in Queue" */
    SEL_UIM_NP_UPNEXT       /* val: the queued song's file name */
} sel_uim_np_kind;

typedef struct {
    sel_uim_np_kind kind;
    char key[32];
    char val[256];
    const char* ref; /* SEL_UIM_NP_MESSAGE, SEL_UIM_NP_PAIR: text too long for val, valid as long as the state it was built from */
    int32_t liked, rating;
} sel_uim_np_line;

/* Builds the panel lines from the model's frame and the engine snapshot, in display order. Returns the line count (<= max, and
 * <= SEL_UIM_NP_LINES_MAX). With no song: the context heading, the (empty) title, the like row and Up Next. */
uint32_t sel_uim_np_lines(const sel_uim* m, const sel_state* st, sel_uim_np_line* out, uint32_t max);
/* The first line of a kind at or after `from`, NULL when there is none. */
const sel_uim_np_line* sel_uim_np_find(const sel_uim_np_line* lines, uint32_t n, sel_uim_np_kind kind, uint32_t from);

#ifdef __cplusplus
}
#endif
#endif
