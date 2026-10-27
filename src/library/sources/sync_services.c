/* Engine side of the net and catalogue host services (I517 D2). A source sees only sel_net and sel_catalogue; this
 * file turns the calls into sync_net.c requests and library requests. */
#include "sync_host.h"

#include "../cache.h"
#include "../local.h"
#include "../net/http.h"
#include "sync_net.h"
#include "sync_util.h"

#include <selecta/catalogue.h>
#include <selecta/engine.h>
#include <selecta/net.h>
#include <stdio.h>
#include <string.h>

#define BATCH_ROWS 128u
#define BATCH_ARENA (BATCH_ROWS * 384u)

typedef struct sel_cat_batch {
    char source[SEL_SOURCE_ID_MAX + 1u];
    sel_scan_row rows[BATCH_ROWS];
    uint32_t count;
    uint32_t used;
    uint32_t batches;
    int failed;
    char arena[BATCH_ARENA];
} sel_cat_batch;

/* ---- net ---- */

static int net_get_file(const sel_host* host, const char* url, const char* path, uint32_t* requests,
                        unsigned long* bytes) {
    (void)host;
    return sel_sync_net_get_file(url, path, requests, bytes);
}

static int net_get_range(const sel_host* host, const char* url, const char* path, unsigned long from,
                         unsigned long to, uint32_t* requests, unsigned long* bytes) {
    (void)host;
    return sel_sync_net_get_range(url, path, from, to, requests, bytes);
}

static int net_head_length(const sel_host* host, const char* url, uint32_t* requests, unsigned long* length) {
    (void)host;
    return sel_sync_net_head_length(url, requests, length);
}

static void net_pause(const sel_host* host, uint32_t ms) {
    sel_sync_net_pause(host, ms);
}

typedef struct mem_sink {
    uint8_t* buf;
    uint32_t cap, got;
} mem_sink;

static int mem_sink_write(void* ctx, const void* data, size_t len) {
    mem_sink* m = ctx;
    size_t room = m->cap - m->got;

    memcpy(m->buf + m->got, data, len < room ? len : room);
    m->got += (uint32_t)(len < room ? len : room);
    return 1;
}

static int net_request(const sel_host* host, const char* url, void* buf, uint32_t cap, uint32_t* got,
                       uint32_t* requests) {
    sel_http_req req;
    sel_http_res res;
    mem_sink m;

    (void)host;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    m.buf = buf;
    m.cap = cap;
    m.got = 0;
    req.url = url;
    (void)sel_http_request(&req, mem_sink_write, &m, &res);
    if (got != NULL) {
        *got = m.got;
    }
    if (requests != NULL) {
        *requests += res.requests;
    }
    return res.status;
}

static const sel_net g_net = {net_get_file, net_get_range, net_head_length, net_pause, net_request};

static const sel_net* host_net(const sel_host* host) {
    (void)host;
    return &g_net;
}

/* ---- catalogue ---- */

static sel_sync_ctx* ctx_of(const sel_host* host) {
    return (sel_sync_ctx*)host->ctx;
}

static int cat_flush(sel_sync_ctx* c) {
    sel_cat_batch* b = c->batch;

    return sel_sync_flush_scan(&c->scfg, b->rows, &b->count, &b->batches, &b->failed) && (b->used = 0, 1);
}

static int cat_begin(const sel_host* host, const char* source_id) {
    sel_sync_ctx* c = ctx_of(host);

    if (c->batch != NULL || c->scfg.lib == NULL || source_id == NULL || strlen(source_id) > SEL_SOURCE_ID_MAX) {
        return 0;
    }
    c->batch = host->mem_alloc(host, sizeof *c->batch, SEL_MEM_FAST);
    if (c->batch == NULL) {
        return 0;
    }
    memset(c->batch, 0, sizeof *c->batch);
    strcpy(c->batch->source, source_id);
    return 1;
}

static const char* arena_copy(sel_cat_batch* b, const char* s) {
    size_t n;
    char* p;

    if (s == NULL) {
        s = "";
    }
    n = strlen(s) + 1u;
    if (n > BATCH_ARENA - b->used) {
        return NULL;
    }
    p = b->arena + b->used;
    memcpy(p, s, n);
    b->used += (uint32_t)n;
    return p;
}

static int fill_row(sel_cat_batch* b, sel_scan_row* r, const sel_catalogue_song* s) {
    r->source = b->source;
    r->path = arena_copy(b, s->path);
    r->format = arena_copy(b, s->format);
    r->artist = arena_copy(b, s->artist);
    r->album = arena_copy(b, s->album);
    r->name = arena_copy(b, s->name);
    r->md5 = arena_copy(b, s->md5);
    r->size = s->size;
    r->duration_ms = s->duration_ms;
    r->year = s->year;
    return r->path && r->format && r->artist && r->album && r->name && r->md5;
}

static int cat_song(const sel_host* host, const sel_catalogue_song* s) {
    sel_sync_ctx* c = ctx_of(host);
    sel_cat_batch* b = c->batch;
    size_t need;

    if (b == NULL || s == NULL) {
        return 0;
    }
    need = 6u;
    need += (s->path ? strlen(s->path) : 0) + (s->format ? strlen(s->format) : 0) + (s->artist ? strlen(s->artist) : 0) +
            (s->album ? strlen(s->album) : 0) + (s->name ? strlen(s->name) : 0) + (s->md5 ? strlen(s->md5) : 0);
    if (need > BATCH_ARENA) {
        return 0;
    }
    if (b->count == BATCH_ROWS || need > BATCH_ARENA - b->used) {
        if (!cat_flush(c)) {
            return 0;
        }
    }
    return fill_row(b, &b->rows[b->count++], s);
}

static int cat_remove(const sel_host* host, const char* const* paths, uint32_t count) {
    sel_sync_ctx* c = ctx_of(host);
    sel_remove_batch rb;

    if (c->batch == NULL || !cat_flush(c)) {
        return 0;
    }
    if (count == 0) {
        return 1;
    }
    rb.source = c->batch->source;
    rb.paths = paths;
    rb.count = count;
    if (!sel_sync_request(&c->scfg, SEL_LIB_REQ_REMOVE, &rb)) {
        c->batch->failed = 1;
        return 0;
    }
    return 1;
}

static int cat_move(const sel_host* host, const char* from, const sel_catalogue_song* to) {
    sel_sync_ctx* c = ctx_of(host);
    sel_scan_row row;
    sel_move mv;

    if (c->batch == NULL || to == NULL || !cat_flush(c)) {
        return 0;
    }
    memset(&row, 0, sizeof row);
    row.source = c->batch->source;
    row.path = to->path ? to->path : "";
    row.format = to->format ? to->format : "";
    row.artist = to->artist ? to->artist : "";
    row.album = to->album ? to->album : "";
    row.name = to->name ? to->name : "";
    row.md5 = to->md5 ? to->md5 : "";
    row.size = to->size;
    row.duration_ms = to->duration_ms;
    row.year = to->year;
    mv.source = c->batch->source;
    mv.from = from;
    mv.to = &row;
    if (!sel_sync_request(&c->scfg, SEL_LIB_REQ_MOVE, &mv)) {
        c->batch->failed = 1;
        return 0;
    }
    return 1;
}

static void cat_end(const sel_host* host) {
    sel_sync_ctx* c = ctx_of(host);

    if (c->batch != NULL) {
        host->mem_free(host, c->batch);
        c->batch = NULL;
    }
}

static int cat_commit(const sel_host* host) {
    sel_sync_ctx* c = ctx_of(host);
    int ok;

    if (c->batch == NULL) {
        return 0;
    }
    ok = cat_flush(c) && !c->batch->failed;
    cat_end(host);
    return ok;
}

static void cat_abort(const sel_host* host) {
    cat_end(host);
}

static int cat_cache_dir(const sel_host* host, const char* source_id, char* out, uint32_t n) {
    sel_cache cache;

    (void)host;
    sel_cache_init(&cache, NULL, 0);
    return snprintf(out, n, "%s/sync/%s", cache.dir, source_id) < (int)n && sel_config_mkdir(out);
}


static int cat_list_dir(const sel_host* host, const char* path, const sel_catalogue_dir_sink* sink) {
    sel_browse b;
    uint32_t i;
    int ok;

    if (sink == NULL || !sel_browse_init(&b, host, 4096u, 256u * 1024u)) {
        return 0;
    }
    ok = sel_browse_list(&b, path);
    for (i = 0; ok && i < b.count; i++) {
        if (sink->entry(sink->ctx, b.text + b.entries[i].name_offset, b.entries[i].is_dir != 0)) {
            break;
        }
    }
    sel_browse_free(&b, host);
    return ok;
}

static int64_t cat_now(const sel_host* host) {
    return ctx_of(host)->scfg.now;
}

static int32_t cat_annotate(const sel_host* host, const sel_catalogue_note* notes, uint32_t count) {
    sel_sync_ctx* c = ctx_of(host);
    sel_annotation* a;
    sel_sticker* st;
    sel_annotate_batch b;
    sel_message reply;
    uint32_t i, j, total = 0, used = 0;
    int32_t matched = -1;

    if (c->batch == NULL || notes == NULL || !cat_flush(c)) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        total += notes[i].sticker_n;
    }
    a = host->mem_alloc(host, (size_t)(count + 1u) * sizeof *a, SEL_MEM_FAST);
    st = host->mem_alloc(host, (size_t)(total + 1u) * sizeof *st, SEL_MEM_FAST);
    if (a != NULL && st != NULL) {
        for (i = 0; i < count; i++) {
            a[i].path = notes[i].path;
            a[i].md5 = notes[i].md5;
            a[i].duration_ms = notes[i].duration_ms;
            a[i].stickers = st + used;
            a[i].sticker_n = notes[i].sticker_n;
            for (j = 0; j < notes[i].sticker_n; j++, used++) {
                st[used].name = notes[i].stickers[j].name;
                st[used].value = notes[i].stickers[j].value;
            }
        }
        b.source = c->batch->source;
        b.items = a;
        b.count = count;
        memset(&reply, 0, sizeof reply);
        if (sel_library_request(c->scfg.lib, SEL_LIB_REQ_ANNOTATE, &b, &reply) && reply.args[1].v.i == 0) {
            matched = (int32_t)reply.args[0].v.i;
        } else {
            c->batch->failed = 1;
        }
    }
    if (a != NULL) {
        host->mem_free(host, a);
    }
    if (st != NULL) {
        host->mem_free(host, st);
    }
    return matched;
}

static const sel_catalogue g_catalogue = {cat_begin,    cat_song,      cat_remove,   cat_move,  cat_commit,
                                          cat_abort,    cat_cache_dir, cat_list_dir, cat_now, cat_annotate};

static const sel_catalogue* host_catalogue(const sel_host* host) {
    return ctx_of(host)->scfg.lib != NULL ? &g_catalogue : NULL;
}

void sel_sync_host_bind(sel_host* out, const sel_host* base, sel_sync_ctx* ctx) {
    *out = *base;
    out->ctx = ctx;
    out->net = host_net;
    out->catalogue = host_catalogue;
}

void sel_sync_ctx_release(sel_sync_ctx* ctx) {
    if (ctx->batch != NULL) {
        sel_host h;
        const sel_host* base = ctx->scfg.host;

        sel_sync_host_bind(&h, base, ctx);
        cat_end(&h);
    }
}
