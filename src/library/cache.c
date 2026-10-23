/* File cache, see cache.h. */
#include "cache.h"

#include "md5.h"

#include <selecta/engine.h>
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
} sink;

static int on_body(void* ctx, const void* data, size_t len) {
    sink* s = ctx;
    sel_md5_update(&s->md5, data, len);
    return s->f != NULL && fwrite(data, 1, len, s->f) == len;
}

static void on_restart(void* ctx) {
    sink* s = ctx;
    if (s->f != NULL) {
        fclose(s->f);
    }
    s->f = fopen(s->part, "wb");
    sel_md5_init(&s->md5);
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
    int ok;

    if (!sel_cache_path(c, source, path, out, cap) || strlen(out) + 6 > sizeof part) {
        return SEL_CACHE_FAILED;
    }
    if (file_exists(out)) {
        c->stats.cache_hits++;
        return SEL_CACHE_OK;
    }
    set = set_for(c, source);
    if (set == NULL || !make_parent(out)) {
        return SEL_CACHE_FAILED;
    }
    snprintf(part, sizeof part, "%s.part", out);
    memset(&s, 0, sizeof s);
    s.part = part;
    s.want = (md5 != NULL && strlen(md5) == 32u) ? md5 : NULL;
    on_restart(&s);
    if (s.f == NULL) {
        return SEL_CACHE_FAILED;
    }
    ok = sel_mirrors_fetch_verified(set, path, on_body, on_restart, on_verify, &s, &res);
    c->stats.mirror_tries += res.tries;
    c->stats.md5_mismatches += res.rejected;
    if (s.f != NULL) {
        fclose(s.f);
    }
    if (ok && rename(part, out) == 0) {
        c->stats.fetches++;
        return SEL_CACHE_OK;
    }
    remove(part);
    return res.rejected > 0 ? SEL_CACHE_MD5 : SEL_CACHE_FAILED;
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
    for (i = 0; i < n; i++) {
        if (sel_cache_fetch(c, source, comp[i], NULL, cpath, sizeof cpath) == SEL_CACHE_OK) {
            c->stats.companions++;
        }
    }
    return r;
}
