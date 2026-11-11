/* I1306b: collection page song titles from one posted song_briefs read: see include/selecta/uimodel/briefs.h. */
#include <string.h>
#include <selecta/uimodel/briefs.h>

size_t sel_uim_briefs_cap(const sel_collection_page* pg) {
    return pg != NULL ? (size_t)pg->songs * 448u + 16u : 0u;
}

int sel_uim_briefs_take(sel_uim_briefs* b, sel_engine* e) {
    uint32_t done = 0;
    int ok = 0;
    if (!b->inflight || b->buf == NULL || !sel_engine_song_briefs_take(e, b->buf, b->cap, &done, &ok)) {
        return 0;
    }
    b->inflight = 0u;
    b->serial = ok ? b->post : 0u; /* a read that did not run is posted again on a later turn */
    b->done = ok ? done : 0u;
    return 1;
}

int sel_uim_briefs_due(const sel_uim_briefs* b, sel_engine* e, const sel_collection_page* pg) {
    return !b->inflight && pg != NULL && pg->rows != NULL && pg->songs != 0u && pg->serial != 0u && pg->serial != b->serial &&
           pg->drill != (uint32_t)SEL_DRILL_PAGE_ROOT && pg->drill != (uint32_t)SEL_DRILL_PAGE_GROUPS && !sel_engine_song_briefs_busy(e);
}

int sel_uim_briefs_post(sel_uim_briefs* b, sel_engine* e, const sel_collection_page* pg) {
    const char* r;
    size_t len = 0;
    uint32_t k;
    b->serial = 0u;
    b->done = 0u;
    if (b->buf == NULL || pg == NULL || pg->rows == NULL) {
        return 0;
    }
    /* rows are "key\0plays\0"; the post wants the keys alone, so they are copied into the answer buffer first */
    for (k = 0, r = pg->rows; k < pg->songs && *r != '\0'; k++) {
        size_t n = strlen(r) + 1u;
        if (len + n + 1u > b->cap) {
            break;
        }
        memcpy(b->buf + len, r, n);
        len += n;
        r += n;
        r += strlen(r) + 1u;
    }
    if (len == 0u || !sel_engine_song_briefs_post(e, b->buf, len, b->cap)) {
        return 0;
    }
    b->post = pg->serial;
    b->inflight = 1u;
    b->posts++;
    return 1;
}

const char* sel_uim_briefs_title(const sel_uim_briefs* b, const sel_collection_page* pg, uint32_t row) {
    const char* t;
    uint32_t k, f;
    if (b == NULL || b->buf == NULL || b->inflight || pg == NULL || b->serial == 0u || pg->serial != b->serial || row >= b->done) {
        return NULL;
    }
    for (t = b->buf, k = 0; k < row; k++) {
        for (f = 0; f < 4u; f++) {
            t += strlen(t) + 1u;
        }
    }
    return t[0] != '\0' ? t : NULL;
}
