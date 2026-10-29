/* File cache, see cache.h. */
#include "cache.h"

#include "dir.h"
#include "md5.h"
#include "net/net.h"
#include "source.h"

#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sel_cache_init(sel_cache* c, const char* dir, int amiga_route) {
    const char* env;
    memset(c, 0, sizeof *c);
    c->amiga_route = amiga_route;
    if (dir == NULL || dir[0] == '\0') {
        dir = getenv("SELECTA_CACHE_DIR");
    }
    if (dir != NULL && dir[0] != '\0') {
        snprintf(c->dir, sizeof c->dir, "%s", dir);
    } else if ((env = getenv("XDG_CACHE_HOME")) != NULL && env[0] != '\0') {
        snprintf(c->dir, sizeof c->dir, "%s/selecta", env);
    } else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
        snprintf(c->dir, sizeof c->dir, "%s/.cache/selecta", env);
    } else {
        snprintf(c->dir, sizeof c->dir, "selecta-cache");
    }
}

static int safe_rel(const char* p) {
    const char* q = p;
    if (*q == '\0') {
        return 0;
    }
    while (*q != '\0') {
        const char* e = strchr(q, '/');
        size_t n = e != NULL ? (size_t)(e - q) : strlen(q);
        if (n == 2 && q[0] == '.' && q[1] == '.') {
            return 0;
        }
        q += n + (e != NULL ? 1u : 0u);
    }
    return strchr(p, '\\') == NULL && strchr(p, ':') == NULL;
}

int sel_cache_path(const sel_cache* c, const char* source, const char* path, char* out, size_t cap) {
    int n;
    while (*path == '/') {
        path++;
    }
    if (!safe_rel(path) || !safe_rel(source)) {
        return 0;
    }
    n = snprintf(out, cap, "%s/files/%s/%s", c->dir, source, path);
    return n > 0 && (size_t)n < cap;
}

/* mkdir -p of the directory part of `file`, through the portable helper in src/core. */
static int make_parent(const char* file) {
    char dir[SEL_CACHE_PATH_MAX];
    const char* slash = strrchr(file, '/');
    size_t n;
    if (slash == NULL) {
        return 1;
    }
    n = (size_t)(slash - file);
    memcpy(dir, file, n);
    dir[n] = '\0';
    return sel_config_mkdir(dir);
}

/* A failed fetch leaves no empty folders behind (I695a): removes the now empty directories above `file`, one per
 * folder level of `path`, never the source's own folder. remove() takes empty directories only, so a folder that
 * holds another song stays. */
static void drop_empty_parents(const char* file, const char* path) {
    char dir[SEL_CACHE_PATH_MAX];
    const char* p;
    size_t n = strlen(file);

    if (n >= sizeof dir) {
        return;
    }
    memcpy(dir, file, n + 1u);
    for (p = path; (p = strchr(p, '/')) != NULL; p++) {
        char* slash = strrchr(dir, '/');

        if (slash == NULL) {
            return;
        }
        *slash = '\0';
        if (remove(dir) != 0) {
            return;
        }
    }
}

static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

typedef struct sink {
    FILE* f;
    const char* part;
    sel_md5 md5;
    const char* want;
    volatile int* cancel;
    unsigned long offset;      /* body bytes written to the part file */
    unsigned long deadline;    /* sel_net_now_ms() at which a sliced fetch yields, 0 = never */
    volatile int yielded;      /* the sink stopped the transfer at the deadline */
    volatile int resumable;    /* the attempt runs over HTTP: a yield can be resumed */
} sink;

static int on_body(void* ctx, const void* data, size_t len) {
    sink* s = ctx;
    if (s->cancel != NULL && *s->cancel) {
        return 0;
    }
    sel_md5_update(&s->md5, data, len);
    if (s->f == NULL || fwrite(data, 1, len, s->f) != len) {
        return 0;
    }
    s->offset += len;
    if (s->deadline != 0 && s->resumable && sel_net_now_ms() >= s->deadline) {
        s->yielded = 1; /* chunk boundary: the bytes are on disk and hashed, the next call resumes after them */
        return 0;
    }
    return 1;
}

static void on_restart(void* ctx) {
    sink* s = ctx;
    if (s->f != NULL) {
        fclose(s->f);
    }
    s->f = (s->cancel != NULL && *s->cancel) ? NULL : fopen(s->part, "wb");
    sel_md5_init(&s->md5);
    s->offset = 0;
    s->yielded = 0;
}

static int on_verify(void* ctx) {
    sink* s = ctx;
    char hex[33];
    sel_md5 m = s->md5;
    if (s->f != NULL && fflush(s->f) != 0) {
        return 0;
    }
    if (s->want == NULL) {
        return 1;
    }
    sel_md5_hex(&m, hex);
    return strcmp(hex, s->want) == 0;
}

static sel_mirror_set* set_for(sel_cache* c, const char* source) {
    unsigned i;
    for (i = 0; i < c->set_count; i++) {
        if (strcmp(c->sets[i].source, source) == 0) {
            return &c->sets[i];
        }
    }
    if (c->set_count >= SEL_CACHE_SETS || sel_mirrors_init(&c->sets[c->set_count], source, c->amiga_route) == 0) {
        return NULL;
    }
    return &c->sets[c->set_count++];
}

sel_cache_result sel_cache_fetch(sel_cache* c, const char* source, const char* path, const char* md5, char* out,
                                 size_t cap) {
    char part[SEL_CACHE_PATH_MAX + 8];
    sel_mirror_set* set;
    sel_mirror_res res;
    sink s;
    sel_mirror_opts o;
    unsigned long from = 0;
    int ok;

    if (!sel_cache_path(c, source, path, out, cap) || strlen(out) + 10 > sizeof part) {
        return SEL_CACHE_FAILED;
    }
    if (file_exists(out)) {
        c->stats.cache_hits++;
        sel_file_touch(out);
        return SEL_CACHE_OK;
    }
    set = set_for(c, source);
    if (set == NULL || !make_parent(out)) {
        return SEL_CACHE_FAILED;
    }
    snprintf(part, sizeof part, "%s%s.part", out, c->part_tag != NULL ? c->part_tag : "");
    memset(&s, 0, sizeof s);
    s.part = part;
    s.cancel = c->cancel;
    s.want = (md5 != NULL && strlen(md5) == 32u) ? md5 : NULL;
    if (c->slice.active && strcmp(c->slice.key, out) != 0) {
        sel_cache_slice_drop(c); /* a partial download of another file is not wanted any more */
    }
    if (c->slice.active && c->slice.offset > 0) {
        /* adopt the partial download (a prefetch that ran out of its slice, or a foreground fetch taking it over) */
        s.f = fopen(part, "ab");
        s.md5 = c->slice.md5;
        s.offset = c->slice.offset;
        from = c->slice.offset;
    } else {
        on_restart(&s);
    }
    if (s.f == NULL) {
        sel_cache_slice_drop(c);
        return SEL_CACHE_FAILED;
    }
    memset(&o, 0, sizeof o);
    o.from = from;
    o.yielded = &s.yielded;
    o.resumable = &s.resumable;
    if (c->slice_ms > 0) {
        s.deadline = sel_net_now_ms() + c->slice_ms;
        o.stall_ms = SEL_CACHE_SLICE_STALL_MS;
        o.max_tries = 1;
    }
    ok = sel_mirrors_fetch_opts(set, path, &o, on_body, on_restart, on_verify, &s, &res);
    c->stats.mirror_tries += res.tries;
    c->stats.md5_mismatches += res.rejected;
    c->last_status = ok ? 0 : res.status;
    if (s.f != NULL) {
        fclose(s.f);
    }
    if (ok && rename(part, out) == 0) {
        c->slice.active = 0;
        c->slice.fails = 0;
        c->stats.fetches++;
        return SEL_CACHE_OK;
    }
    if (!ok && s.yielded) {
        c->slice.active = 1;
        snprintf(c->slice.key, sizeof c->slice.key, "%s", out);
        c->slice.offset = s.offset;
        c->slice.md5 = s.md5;
        return SEL_CACHE_PENDING;
    }
    if (c->slice_ms > 0 && !ok && ++c->slice.fails < set->count) {
        /* one mirror per call: the next call tries the next ranked mirror from byte zero */
        c->slice.active = 1;
        snprintf(c->slice.key, sizeof c->slice.key, "%s", out);
        c->slice.offset = 0;
        remove(part);
        return SEL_CACHE_PENDING;
    }
    c->slice.fails = 0;
    c->slice.active = 0;
    remove(part);
    drop_empty_parents(out, path);
    return res.rejected > 0 ? SEL_CACHE_MD5 : SEL_CACHE_FAILED;
}

void sel_cache_slice_drop(sel_cache* c) {
    if (c->slice.active) {
        char part[SEL_CACHE_PATH_MAX + 8];
        if (strlen(c->slice.key) + 10 <= sizeof part) {
            snprintf(part, sizeof part, "%s%s.part", c->slice.key, c->part_tag != NULL ? c->part_tag : "");
            remove(part);
        }
    }
    c->slice.active = 0;
    c->slice.offset = 0;
    c->slice.fails = 0;
}

/* ---- companions ------------------------------------------------------ */

static int ci_eq(const char* a, const char* b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return 0;
        }
    }
    return 1;
}

/* Does `name` match `pattern` (one optional '*')? stem receives the part under the star. */
static int match(const char* pattern, size_t plen, const char* name, const char** stem, size_t* slen) {
    const char* star = memchr(pattern, '*', plen);
    size_t pre, suf, nlen = strlen(name);
    if (star == NULL) {
        *stem = name;
        *slen = 0;
        return nlen == plen && ci_eq(pattern, name, plen);
    }
    pre = (size_t)(star - pattern);
    suf = plen - pre - 1u;
    if (nlen < pre + suf || !ci_eq(pattern, name, pre) || !ci_eq(star + 1, name + nlen - suf, suf)) {
        return 0;
    }
    *stem = name + pre;
    *slen = nlen - pre - suf;
    return 1;
}

unsigned sel_companion_paths(const char* rules, const char* path, char out[][SEL_CACHE_PATH_MAX], unsigned max) {
    const char* base = strrchr(path, '/');
    size_t dirlen;
    unsigned n = 0;
    const char* r = rules;

    base = base != NULL ? base + 1 : path;
    dirlen = (size_t)(base - path);
    while (r != NULL && *r != '\0' && n < max) {
        const char* end = strchr(r, ';');
        size_t rlen = end != NULL ? (size_t)(end - r) : strlen(r);
        const char* gt = memchr(r, '>', rlen);
        const char* stem;
        size_t slen;
        if (gt != NULL && match(r, (size_t)(gt - r), base, &stem, &slen)) {
            const char* tpl = gt + 1;
            size_t tlen = rlen - (size_t)(gt - r) - 1u;
            size_t o = 0, i;
            char* dst = out[n];
            int fits = dirlen < SEL_CACHE_PATH_MAX;
            if (fits) {
                memcpy(dst, path, dirlen);
                o = dirlen;
            }
            for (i = 0; fits && i < tlen; i++) {
                if (tpl[i] == '*') {
                    fits = o + slen < SEL_CACHE_PATH_MAX;
                    if (fits) {
                        memcpy(dst + o, stem, slen);
                        o += slen;
                    }
                } else {
                    fits = o + 1u < SEL_CACHE_PATH_MAX;
                    if (fits) {
                        dst[o++] = tpl[i];
                    }
                }
            }
            if (fits) {
                dst[o] = '\0';
                n++;
            }
        }
        r = end != NULL ? end + 1 : NULL;
    }
    return n;
}

void sel_companion_rules(uint32_t (*registry_count)(void), const sel_tag* (*registry_get)(uint32_t), char* out,
                         size_t cap) {
    uint32_t i, n = registry_count != NULL ? registry_count() : 0;
    size_t used = 0;
    if (cap == 0) {
        return;
    }
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        const sel_tag* desc = registry_get(i);
        const sel_tag* hooks;
        const char* rules;
        size_t l;
        if (desc == NULL || sel_tag_find(desc, SEL_PLUGIN_KIND, 0) != SEL_PLUGIN_KIND_REPLAYER) {
            continue;
        }
        hooks = (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0);
        rules = hooks != NULL ? (const char*)sel_tag_find(hooks, SEL_RP_COMPANIONS, 0) : NULL;
        l = rules != NULL ? strlen(rules) : 0;
        if (l == 0 || used + l + 2u > cap) {
            continue;
        }
        if (used != 0) {
            out[used++] = ';';
        }
        memcpy(out + used, rules, l);
        used += l;
        out[used] = '\0';
    }
}

sel_cache_result sel_cache_fetch_song(sel_cache* c, const char* source, const char* path, const char* md5,
                                      const char* rules, char* out, size_t cap) {
    char comp[SEL_CACHE_COMPANIONS_MAX][SEL_CACHE_PATH_MAX];
    char cpath[SEL_CACHE_PATH_MAX];
    unsigned n, i;
    sel_cache_result r = sel_cache_fetch(c, source, path, md5, out, cap);

    if (r != SEL_CACHE_OK || rules == NULL) {
        return r;
    }
    n = sel_companion_paths(rules, path, comp, SEL_CACHE_COMPANIONS_MAX);
    for (i = 0; i < n && !(c->cancel != NULL && *c->cancel); i++) {
        sel_cache_result cr = sel_cache_fetch(c, source, comp[i], NULL, cpath, sizeof cpath);
        if (cr == SEL_CACHE_PENDING) {
            return SEL_CACHE_PENDING; /* the next call finds the song cached and resumes this companion */
        }
        if (cr == SEL_CACHE_OK) {
            c->stats.companions++;
        }
    }
    return r;
}

/* ---- trim (I167) ----------------------------------------------------- */

typedef struct trim_entry {
    char* path; /* full path under <dir>/files */
    uint32_t size;
    int64_t mtime;
    int exempt;
} trim_entry;

typedef struct trim_walk {
    trim_entry* e;
    unsigned n, cap;
    int failed;
} trim_walk;

typedef struct trim_ctx {
    trim_walk* w;
    char dir[SEL_CACHE_PATH_MAX];
} trim_ctx;

static void walk(trim_walk* w, const char* dir);

static int on_entry(void* vctx, const char* name, int is_dir) {
    trim_ctx* t = vctx;
    trim_walk* w = t->w;
    char full[SEL_CACHE_PATH_MAX];
    size_t nl = strlen(name);
    uint32_t size;
    int64_t mtime;

    if (snprintf(full, sizeof full, "%s/%s", t->dir, name) >= (int)sizeof full) {
        return 1;
    }
    if (is_dir) {
        walk(w, full);
        return !w->failed;
    }
    if ((nl > 5 && strcmp(name + nl - 5, ".part") == 0) || !sel_file_stat(full, &size, &mtime)) {
        return 1;
    }
    if (w->n == w->cap) {
        unsigned cap = w->cap ? w->cap * 2u : 64u;
        trim_entry* ne = realloc(w->e, cap * sizeof *ne);
        if (ne == NULL) {
            w->failed = 1;
            return 0;
        }
        w->e = ne;
        w->cap = cap;
    }
    w->e[w->n].path = malloc(strlen(full) + 1u);
    if (w->e[w->n].path == NULL) {
        w->failed = 1;
        return 0;
    }
    strcpy(w->e[w->n].path, full);
    w->e[w->n].size = size;
    w->e[w->n].mtime = mtime;
    w->e[w->n].exempt = 0;
    w->n++;
    return 1;
}

static void walk(trim_walk* w, const char* dir) {
    trim_ctx t;

    t.w = w;
    snprintf(t.dir, sizeof t.dir, "%s", dir);
    sel_dir_each(dir, on_entry, &t);
}

static int by_age(const void* a, const void* b) {
    const trim_entry* x = a;
    const trim_entry* y = b;
    return x->mtime < y->mtime ? -1 : x->mtime > y->mtime ? 1 : 0;
}

int sel_cache_is_remote(const char* key) {
    char id[SEL_SOURCE_ID_MAX];
    const char* colon = key != NULL ? strchr(key, ':') : NULL;
    size_t n = colon != NULL ? (size_t)(colon - key) : 0;

    if (n == 0 || n >= sizeof id || colon[1] == '/' || colon[1] == '\0') { /* no source id, a URL scheme or no path */
        return 0;
    }
    memcpy(id, key, n);
    id[n] = '\0';
    return sel_source_is_remote(id);
}

uint32_t sel_cache_budget(const sel_tag* machine) {
    uint64_t fast = (uint64_t)sel_tag_find(machine, SEL_MACHINE_FAST_RAM, 0);

    if (fast >= 64ull * 1024u * 1024u) {
        return 512u * 1024u * 1024u;
    }
    if (fast >= 8ull * 1024u * 1024u) {
        return 64u * 1024u * 1024u;
    }
    return 8u * 1024u * 1024u;
}

/* Exempt the companions of every exempt entry: entry `<source>/<path>` -> sel_companion_paths(path). */
static void mark_companions(trim_walk* w, size_t rl, const char* rules, sel_cache_trim_result* out) {
    char comp[SEL_CACHE_COMPANIONS_MAX][SEL_CACHE_PATH_MAX];
    unsigned i, j, k, n;

    for (i = 0; i < w->n; i++) {
        const char* rel = w->e[i].path + rl + 1u;
        const char* slash = strchr(rel, '/');
        size_t sl;

        if (!w->e[i].exempt || slash == NULL) {
            continue;
        }
        sl = (size_t)(slash - rel);
        n = sel_companion_paths(rules, slash + 1, comp, SEL_CACHE_COMPANIONS_MAX);
        for (k = 0; k < n; k++) {
            for (j = 0; j < w->n; j++) {
                const char* r2 = w->e[j].path + rl + 1u;

                if (!w->e[j].exempt && strncmp(r2, rel, sl + 1u) == 0 && strcmp(r2 + sl + 1u, comp[k]) == 0) {
                    w->e[j].exempt = 1;
                    out->companion_exempt++;
                }
            }
        }
    }
}

void sel_cache_trim(sel_cache* c, uint32_t budget, sel_cache_keep_fn keep, void* ctx, const char* protect,
                    const char* rules, sel_cache_trim_result* out) {
    trim_walk w;
    char root[SEL_CACHE_PATH_MAX];
    size_t rl;
    unsigned i;
    uint64_t total = 0;

    memset(&w, 0, sizeof w);
    memset(out, 0, sizeof *out);
    c->stats.trim_runs++;
    rl = (size_t)snprintf(root, sizeof root, "%s/files", c->dir);
    walk(&w, root);
    for (i = 0; i < w.n; i++) {
        const char* rel = w.e[i].path + rl + 1u;
        char key[SEL_CACHE_PATH_MAX];
        char* slash;

        total += w.e[i].size;
        snprintf(key, sizeof key, "%s", rel);
        slash = strchr(key, '/');
        if (slash != NULL) {
            *slash = ':';
        }
        if (protect != NULL && strcmp(protect, w.e[i].path) == 0) {
            w.e[i].exempt = 1;
        } else if (keep != NULL && keep(ctx, key)) {
            w.e[i].exempt = 1;
            out->kept_exempt++;
        }
    }
    if (rules != NULL && rules[0] != '\0') {
        mark_companions(&w, rl, rules, out);
    }
    out->bytes_before = total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)total;
    if (total > budget && w.n > 0) {
        qsort(w.e, w.n, sizeof *w.e, by_age);
        for (i = 0; i < w.n && total > budget; i++) {
            if (!w.e[i].exempt && remove(w.e[i].path) == 0) {
                total -= w.e[i].size;
                out->trimmed++;
                c->stats.evicted++;
            }
        }
    }
    out->bytes_after = total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)total;
    for (i = 0; i < w.n; i++) {
        free(w.e[i].path);
    }
    free(w.e);
}
