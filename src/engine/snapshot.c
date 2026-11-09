#include "snapshot.h"

#include "../core/count.h"
#include "spsc.h"

#include <stdlib.h>
#include <string.h>
#if !defined(__amigaos__) && !defined(AMIGA)
#include <time.h>
#endif

/* Bump cursor over one blob. A put that does not fit sets `fail`; the caller grows the blob and copies again. */
typedef struct cur {
    char* base;
    uint32_t cap, used;
    int fail;
} cur;

static uint32_t align8(uint32_t n) {
    return (n + 7u) & ~7u;
}

static void* put(cur* c, const void* src, size_t n) {
    uint32_t off = align8(c->used);
    if (c->fail || n > (size_t)0x7FFFFFFFu || off > c->cap || n > (size_t)(c->cap - off)) {
        c->fail = 1;
        return NULL;
    }
    memcpy(c->base + off, src, n);
    c->used = off + (uint32_t)n;
    return c->base + off;
}

static const char* put_str(cur* c, const char* s) {
    return s == NULL ? NULL : (const char*)put(c, s, strlen(s) + 1u);
}

/* Length of `n` records of `fields` NUL-terminated strings. */
static size_t span(const char* blk, uint32_t n, unsigned fields) {
    const char* p = blk;
    uint32_t i;
    for (i = 0; i < n * fields; i++) p += strlen(p) + 1u;
    return (size_t)(p - blk);
}

/* A list of `fields`-string records ended by an empty string: length with the terminator. */
static size_t span_list(const char* blk, unsigned fields) {
    const char* p = blk;
    unsigned k;
    while (*p != '\0') {
        for (k = 0; k < fields; k++) p += strlen(p) + 1u;
    }
    return (size_t)(p - blk) + 1u;
}

/* Composer page: "canon\0display\0songs\0" then "key\0name\0" per song, then "\0". */
static size_t span_composer_page(const char* blk) {
    size_t head = span(blk, 1u, 3u);
    return head + span_list(blk + head, 2u);
}

static const char* put_list(cur* c, const char* blk, size_t len) {
    return blk == NULL ? NULL : (const char*)put(c, blk, len);
}

static int copy_view(cur* c, const sel_view_state* src, sel_view_state* out) {
    sel_view_row* rows = NULL;
    uint32_t i;
    *out = *src;
    out->browse_path = put_str(c, src->browse_path);
    out->browse_leaf = put_str(c, src->browse_leaf);
    out->group = put_str(c, src->group);
    out->drill[0] = put_str(c, src->drill[0]);
    out->drill[1] = put_str(c, src->drill[1]);
    if (src->rows != NULL && src->row_n > 0u) {
        rows = (sel_view_row*)put(c, src->rows, (size_t)src->row_n * sizeof *rows);
        if (rows == NULL) return 0;
        for (i = 0; i < src->row_n; i++) {
            rows[i].text = put_str(c, src->rows[i].text);
            rows[i].key = put_str(c, src->rows[i].key);
            rows[i].art_ref = put_str(c, src->rows[i].art_ref);
            rows[i].artist = put_str(c, src->rows[i].artist);
            rows[i].format = put_str(c, src->rows[i].format);
        }
    }
    out->rows = rows;
    return !c->fail;
}

/* The core blob: everything the pump rewrites every turn. */
static int copy_core(cur* c, sel_state* dst, const sel_state* src) {
    sel_view_state* v;
    sel_arg a;
    memset(&a, 0, sizeof a);
    if (src->song.meta != NULL) {
        c->used = align8(c->used);
        if (!sel_spsc_copy_tags(&a, src->song.meta, c->base, c->cap, &c->used)) {
            c->fail = 1;
            return 0;
        }
        dst->song.meta = a.v.t;
    }
    dst->song.path = put_str(c, src->song.path);
    dst->song.sysfile_label = put_str(c, src->song.sysfile_label);
    dst->song.sysfile_name = put_str(c, src->song.sysfile_name);
    dst->ctx_name = put_str(c, src->ctx_name);
    dst->view = NULL;
    if (src->view != NULL) {
        v = (sel_view_state*)put(c, src->view, sizeof *v);
        if (v == NULL || !copy_view(c, src->view, v)) return 0;
        dst->view = v;
    }
    dst->viz = src->viz != NULL ? (const sel_viz_frame*)put(c, src->viz, sizeof *src->viz) : NULL;
    dst->pattern = src->pattern != NULL ? (const sel_pattern_view*)put(c, src->pattern, sizeof *src->pattern) : NULL;
    dst->panel = put_list(c, src->panel, src->panel != NULL ? span(src->panel, src->panel_rows, 2u) : 0u);
    dst->stats = put_list(c, src->stats, src->stats != NULL ? span_list(src->stats, 2u) : 0u);
    dst->composers = put_list(c, src->composers, src->composers != NULL ? span_list(src->composers, 3u) : 0u);
    dst->formats = put_list(c, src->formats, src->formats != NULL ? span_list(src->formats, 2u) : 0u);
    dst->composer_page = put_list(c, src->composer_page, src->composer_page != NULL ? span_composer_page(src->composer_page) : 0u);
    dst->lyric_text = put_str(c, src->lyric_text);
    dst->lyric_current = put_str(c, src->lyric_current);
    dst->lyric_prev = put_str(c, src->lyric_prev);
    dst->lyric_next = put_str(c, src->lyric_next);
    return !c->fail;
}

static int blob_grow(const sel_host* h, sel_snap_blob* b, uint32_t want) {
    char* n;
    uint32_t cap = b->cap == 0u ? 4096u : b->cap;
    while (cap < want) {
        if (cap > 0x3FFFFFFFu) return 0;
        cap *= 2u;
    }
    if (cap == b->cap) return 1;
    n = (char*)h->mem_alloc(h, cap, SEL_MEM_FAST);
    if (n == NULL) return 0;
    if (b->p != NULL) h->mem_free(h, b->p);
    b->p = n;
    b->cap = cap;
    return 1;
}

static uint64_t now_us(void) {
#if defined(__amigaos__) || defined(AMIGA)
    return 0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
#endif
}

int sel_snap_init(sel_snap* s, const sel_host* host) {
    memset(s, 0, sizeof *s);
    s->host = host;
    s->write = 0;
    s->middle = 1u;
    s->read = 2u;
    return 1;
}

void sel_snap_free(sel_snap* s) {
    uint32_t i;
    if (s->host == NULL) return;
    for (i = 0; i < 3u; i++) {
        if (s->buf[i].core.p != NULL) s->host->mem_free(s->host, s->buf[i].core.p);
        if (s->buf[i].queue.p != NULL) s->host->mem_free(s->host, s->buf[i].queue.p);
        if (s->buf[i].rows.p != NULL) s->host->mem_free(s->host, s->buf[i].rows.p);
        if (s->buf[i].paths.p != NULL) s->host->mem_free(s->host, s->buf[i].paths.p);
    }
    memset(s, 0, sizeof *s);
}

static void relink_aux(sel_snap_aux* a) {
    unsigned k;
    a->stats.replayer = a->has_replayer ? a->s_replayer : NULL;
    a->stats.dup_played = a->has_dup ? a->s_dup : NULL;
    a->stats.archive_member = a->has_member ? a->s_member : NULL;
    a->stats.loudness_source = a->has_loud ? a->s_loud : NULL;
    for (k = 0; k < SEL_STATS_SEARCH_SRC; k++) a->stats.search_src_id[k] = a->has_src[k] ? a->s_src[k] : NULL;
}

/* The accessor answers: a plain copy, then the offsets and the pool into this buffer's own blob. A pool that cannot grow leaves
 * the paths of the previous publish out (path_off NULL: every row answers "none"). */
static void copy_aux(sel_snap* s, sel_snap_buf* b, const sel_snap_aux* aux) {
    if (aux == NULL) {
        memset(&b->aux, 0, sizeof b->aux);
        return;
    }
    b->aux = *aux;
    relink_aux(&b->aux);
    b->aux.path_off = NULL;
    b->aux.path_pool = NULL;
    if (aux->path_off != NULL) {
        uint32_t offb = align8((aux->path_rows + 1u) * (uint32_t)sizeof(uint32_t));
        if (blob_grow(s->host, &b->paths, offb + aux->path_bytes + 8u)) {
            memcpy(b->paths.p, aux->path_off, (aux->path_rows + 1u) * sizeof(uint32_t));
            if (aux->path_bytes > 0u) memcpy(b->paths.p + offb, aux->path_pool, aux->path_bytes);
            b->aux.path_off = (const uint32_t*)(const void*)b->paths.p;
            b->aux.path_pool = b->paths.p + offb;
        } else {
            b->aux.path_rows = 0u;
        }
    }
}

int sel_snap_publish(sel_snap* s, const sel_state* live, const sel_snap_aux* aux) {
    sel_snap_buf* b = &s->buf[s->write];
    uint64_t t0 = now_us(), dt;
    uint32_t bytes, old, qoff = 0;
    sel_state tmp;
    cur c;
    int ok = 0, tries;

    /* queue: entries then text, copied only when the version moved since this buffer last took it */
    if (live->queue != NULL && live->queue_count > 0u) {
        uint32_t qb = live->queue_count * (uint32_t)sizeof(sel_queue_entry), tb = live->queue_text_bytes;
        qoff = align8(qb);
        if (!b->q_valid || b->q_ver != live->queue_version || b->q_count != live->queue_count || b->q_bytes != tb) {
            if (!blob_grow(s->host, &b->queue, qoff + tb + 8u)) return 0;
            memcpy(b->queue.p, live->queue, qb);
            if (tb > 0u) memcpy(b->queue.p + qoff, live->queue_text, tb);
            b->q_ver = live->queue_version;
            b->q_count = live->queue_count;
            b->q_bytes = tb;
            b->q_valid = 1;
        }
    } else {
        b->q_valid = 0;
    }
    /* collection song block: keyed on serial, record count and shape */
    if (live->collection != NULL && live->collection->rows != NULL) {
        const sel_collection_page* pg = live->collection;
        unsigned fields = pg->drill == (uint32_t)SEL_DRILL_PAGE_GROUPS || pg->drill == (uint32_t)SEL_DRILL_PAGE_ROOT ? 3u : 2u;
        uint32_t key[3];
        size_t len = span(pg->rows, pg->songs, fields) + 1u;
        key[0] = pg->serial;
        key[1] = pg->songs;
        key[2] = pg->drill;
        if (!b->r_valid || memcmp(key, b->r_key, sizeof key) != 0) {
            if (!blob_grow(s->host, &b->rows, (uint32_t)len + 8u)) return 0;
            memcpy(b->rows.p, pg->rows, len);
            memcpy(b->r_key, key, sizeof key);
            b->r_len = (uint32_t)len;
            b->r_valid = 1;
        }
    } else {
        b->r_valid = 0;
    }
    c.base = NULL;
    c.cap = c.used = 0;
    c.fail = 0;
    for (tries = 0; tries < 16 && !ok; tries++) {
        if (b->core.p == NULL && !blob_grow(s->host, &b->core, 4096u)) return 0;
        tmp = *live;
        c.base = b->core.p;
        c.cap = b->core.cap;
        c.used = 0;
        c.fail = 0;
        if (copy_core(&c, &tmp, live)) {
            tmp.collection = NULL;
            if (live->collection != NULL) {
                sel_collection_page* pg = (sel_collection_page*)put(&c, live->collection, sizeof *pg);
                if (pg != NULL) {
                    pg->rows = live->collection->rows != NULL ? b->rows.p : NULL;
                    pg->order = live->collection->order != NULL
                                    ? (const uint32_t*)put(&c, live->collection->order, (size_t)live->collection->shown * sizeof(uint32_t))
                                    : NULL;
                    tmp.collection = pg;
                }
            }
            ok = !c.fail;
        }
        if (!ok && !blob_grow(s->host, &b->core, b->core.cap * 2u)) return 0;
    }
    if (!ok) return 0;
    if (b->q_valid) {
        tmp.queue = (const sel_queue_entry*)(const void*)b->queue.p;
        tmp.queue_text = b->queue.p + qoff;
    } else {
        tmp.queue = NULL;
        tmp.queue_text = NULL;
    }
    b->st = tmp;
    copy_aux(s, b, aux);
    bytes = (uint32_t)sizeof b->st + c.used + (b->q_valid ? qoff + b->q_bytes : 0u) + (b->r_valid ? b->r_len : 0u);
    old = SEL_ATOMIC_XCHG(&s->middle, s->write | 4u);
    s->write = old & 3u;
    dt = now_us() - t0;
    SEL_ATOMIC_INC_REL(&s->publishes);
    if (bytes > s->bytes_max) SEL_ATOMIC_STORE_REL(&s->bytes_max, bytes);
    if ((uint32_t)dt > s->us_max) SEL_ATOMIC_STORE_REL(&s->us_max, (uint32_t)dt);
    return 1;
}

int sel_snap_acquire(sel_snap* s) {
    uint32_t old;
    if ((SEL_ATOMIC_LOAD_ACQ(&s->middle) & 4u) == 0u) return 0;
    old = SEL_ATOMIC_XCHG(&s->middle, s->read);
    s->read = old & 3u;
    return 1;
}

const sel_state* sel_snap_state(const sel_snap* s) {
    return &s->buf[s->read].st;
}

const sel_snap_aux* sel_snap_aux_get(const sel_snap* s) {
    return &s->buf[s->read].aux;
}
