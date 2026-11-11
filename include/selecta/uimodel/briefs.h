/* I1306b: the song titles of an open collection page (album, artist, composer, playlist), one model for the text UI and the MUI UI.
 * The titles come in ONE posted song_briefs read per page serial (the desktop pattern, I948a); the read never blocks a UI turn (I829).
 * Until it lands, and for a song without a catalogued title, a row shows the file name of its key.
 *
 * Pure C99, no allocation: the caller owns the answer buffer. Each UI turn:
 *   if (sel_uim_briefs_take(&b, e)) redraw;
 *   if (page on screen && sel_uim_briefs_due(&b, e, pg)) { (re)allocate b.buf to sel_uim_briefs_cap(pg) bytes, set b.cap;
 *                                                           sel_uim_briefs_post(&b, e, pg); }
 * and a song row reads sel_uim_briefs_title(&b, pg, row). */
#ifndef SELECTA_UIMODEL_BRIEFS_H
#define SELECTA_UIMODEL_BRIEFS_H

#include <selecta/engine.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sel_uim_briefs {
    char* buf;         /* caller owned: the keys block while a read is in flight, then the answer ("title\0artist\0album\0album id\0" per song) */
    size_t cap;        /* bytes in buf */
    uint32_t serial;   /* page serial the answer in buf belongs to; 0 none */
    uint32_t post;     /* page serial the read in flight was posted for */
    uint32_t inflight; /* 1 from post until take */
    uint32_t done;     /* songs the answer filled */
    uint32_t posts;    /* reads posted (the one-read-per-page sentinel) */
} sel_uim_briefs;

/* The answer buffer bytes page pg needs (the desktop's bound per song). */
size_t sel_uim_briefs_cap(const sel_collection_page* pg);
/* Takes the read in flight when it is ready. 1 when an answer landed (the caller redraws); a read that did not run clears the answer so
 * the page is posted again on a later turn. */
int sel_uim_briefs_take(sel_uim_briefs* b, sel_engine* e);
/* 1 when page pg wants a read posted now: it has song rows, is not a drill root or group page, has no answer yet and the engine slot is free. */
int sel_uim_briefs_due(const sel_uim_briefs* b, sel_engine* e, const sel_collection_page* pg);
/* Copies pg's song keys into b->buf (b->cap bytes) and posts the read. 1 when posted. */
int sel_uim_briefs_post(sel_uim_briefs* b, sel_engine* e, const sel_collection_page* pg);
/* The catalogued title of stored song row `row` of pg; NULL when the answer is not for pg, did not reach the row, or the song has none. */
const char* sel_uim_briefs_title(const sel_uim_briefs* b, const sel_collection_page* pg, uint32_t row);

#ifdef __cplusplus
}
#endif

#endif
