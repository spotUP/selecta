/* I1222f: the Now Playing panel content, see include/selecta/uimodel/nppanel.h. Taken from the desktop's draw_nowplaying (I619, I1127c, I290). */
#include <selecta/uimodel/nppanel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sel_uim_np_line* add(sel_uim_np_line* out, uint32_t max, uint32_t* n, sel_uim_np_kind kind, const char* key, const char* val) {
    sel_uim_np_line* l;
    if (*n >= max) {
        return NULL;
    }
    l = &out[(*n)++];
    memset(l, 0, sizeof *l);
    l->kind = kind;
    snprintf(l->key, sizeof l->key, "%s", key != NULL ? key : "");
    snprintf(l->val, sizeof l->val, "%s", val != NULL ? val : "");
    return l;
}

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

const sel_uim_np_line* sel_uim_np_find(const sel_uim_np_line* lines, uint32_t n, sel_uim_np_kind kind, uint32_t from) {
    uint32_t i;
    for (i = from; i < n; i++) {
        if (lines[i].kind == kind) {
            return &lines[i];
        }
    }
    return NULL;
}

uint32_t sel_uim_np_lines(const sel_uim* m, const sel_state* st, sel_uim_np_line* out, uint32_t max) {
    uint32_t n = 0, k, first, pairs = 0;
    const char* ctx = st->ctx_name; /* I994e: the engine names the context the playing song started from */
    const char* note;
    sel_uim_np_line* l;
    int liked = 0, rating = 0;
    /* I619: the context heads the panel (Spotify): the playlist, else the release, else the plain heading. */
    if (ctx[0] == '\0' && st->panel != NULL) {
        const char* q = st->panel;
        while (*q != '\0') {
            const char* v = q + strlen(q) + 1;
            if (strcmp(q, "Release") == 0 && v[0] != '\0') {
                ctx = v;
            }
            q = v + strlen(v) + 1;
        }
    }
    add(out, max, &n, SEL_UIM_NP_CONTEXT, "", ctx[0] != '\0' ? ctx : "Now Playing");
    add(out, max, &n, SEL_UIM_NP_TITLE, "", m->frame.title);
    if (m->frame.artist[0] != '\0') {
        add(out, max, &n, SEL_UIM_NP_ARTIST, "", m->frame.artist);
    }
    if (st->panel != NULL) {
        const char* p = st->panel;
        while (*p != '\0') {
            const char* v = p + strlen(p) + 1;
            if (strcmp(p, "Rating") == 0) {
                rating = atoi(v);
            } else if (strcmp(p, "Liked") == 0) {
                liked = 1;
            }
            p = v + strlen(v) + 1;
        }
    }
    l = add(out, max, &n, SEL_UIM_NP_LIKE, "", "");
    if (l != NULL) {
        l->liked = liked;
        l->rating = rating;
    }
    if (st->panel != NULL) {
        const char* p = st->panel;
        while (*p != '\0' && pairs < SEL_UIM_NP_PAIRS_MAX) {
            const char* v = p + strlen(p) + 1;
            add(out, max, &n, SEL_UIM_NP_PAIR, p, v);
            pairs++;
            p = v + strlen(v) + 1;
        }
    }
    note = sel_uim_np_note(&st->song); /* I1127c */
    if (note != NULL) {
        const char* act = sel_uim_np_note_action(&st->song);
        add(out, max, &n, SEL_UIM_NP_NOTE, act != NULL ? act : "", note);
    }
    if (st->lyric_lines > 0u) {
        add(out, max, &n, SEL_UIM_NP_LYRIC, "previous", st->lyric_prev);
        add(out, max, &n, SEL_UIM_NP_LYRIC, "current", st->lyric_current);
        add(out, max, &n, SEL_UIM_NP_LYRIC, "next", st->lyric_next);
    } else if (st->lyric_text != NULL) { /* a tracker song message: several lines, '\n' apart (I290) */
        l = add(out, max, &n, SEL_UIM_NP_MESSAGE, "", "");
        if (l != NULL) {
            l->ref = st->lyric_text;
        }
    }
    /* Up next: the queue entries after the playing one. */
    for (k = 0; k < st->queue_count && st->queue[k].id != st->queue_playing; k++) {
    }
    add(out, max, &n, SEL_UIM_NP_UPNEXT_HDR, "", "Next in Queue");
    for (first = ++k; k < st->queue_count && k - first < SEL_UIM_NP_UPNEXT_MAX; k++) {
        add(out, max, &n, SEL_UIM_NP_UPNEXT, "", basename_of(st->queue_text + st->queue[k].path_offset));
    }
    return n;
}
