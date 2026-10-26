/* Mirrors transport, see mirrors.h. Portable C over the HTTP and FTP clients. */
#include "mirrors.h"

#include "ftp.h"
#include "http.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* https wherever TLS is compiled in (every build but the Amiga, SEL_NO_TLS, see tls.c); the login sends a password. */
#ifdef SEL_NO_TLS
#define DEVILBOX_DEFAULT "http://devilbox.uprough.net"
#else
#define DEVILBOX_DEFAULT "https://devilbox.uprough.net"
#endif
#define ENV_PREFIX "SELECTA_MIRRORS_"

const char* sel_devilbox_base(void) {
    const char* dev = getenv("SELECTA_DEVILBOX_URL");

    return dev != NULL && dev[0] != '\0' ? dev : DEVILBOX_DEFAULT;
}

/* Built-in lists (research 2026-10-25-m3-external-endpoints.md). HTTP mirrors first, FTP last. */
typedef struct builtin {
    const char* source;
    const char* bases;
} builtin;

static const builtin BUILTIN[] = {
    {"modland", "http://ftp.amigascne.org/mirrors/ftp.modland.com/pub/modules/,ftp://ftp.modland.com/pub/modules/"},
    {"hvsc", "https://hvsc.c64.org/download/"},
    {"asma", "https://asma.atari.org/asma/"},
    {"sndh", "https://sndh.atari.org/sndh/sndh_lf/"},
    {"keygenmusic", "https://keygenmusic.tk/kgm/"},
};

static int add_base(sel_mirror_set* s, const char* base, size_t n) {
    char* dst;
    if (n == 0 || s->count >= SEL_MIRROR_MAX || n + 2 > SEL_MIRROR_URL_MAX) {
        return 0;
    }
    dst = s->base[s->count];
    memcpy(dst, base, n);
    if (dst[n - 1] != '/') {
        dst[n++] = '/';
    }
    dst[n] = '\0';
    s->ttfb_ms[s->count] = 0;
    s->failures[s->count] = 0;
    s->count++;
    return 1;
}

static void add_list(sel_mirror_set* s, const char* list) {
    const char* p = list;
    while (*p != '\0') {
        const char* e = strchr(p, ',');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p);
        add_base(s, p, n);
        p += n;
        if (*p == ',') {
            p++;
        }
    }
}

unsigned sel_mirrors_init(sel_mirror_set* s, const char* source, int amiga_route) {
    char envname[48];
    const char* v;
    size_t i, n;
    memset(s, 0, sizeof *s);
    snprintf(s->source, sizeof s->source, "%s", source);
    v = getenv("SELECTA_MIRROR");
    if (v != NULL && v[0] != '\0' && add_base(s, v, strlen(v))) {
        s->home = 1;
    }
    if (amiga_route) {
        char base[SEL_MIRROR_URL_MAX];
        snprintf(base, sizeof base, "%s/api/mirror/%s/", sel_devilbox_base(), source);
        add_base(s, base, strlen(base));
        return s->count;
    }
    n = strlen(ENV_PREFIX);
    memcpy(envname, ENV_PREFIX, n);
    for (i = 0; source[i] != '\0' && n + 1 < sizeof envname; i++) {
        char c = source[i];
        envname[n++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    envname[n] = '\0';
    v = getenv(envname);
    if (v != NULL && v[0] != '\0') {
        add_list(s, v);
        return s->count;
    }
    for (i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++) {
        if (strcmp(BUILTIN[i].source, source) == 0) {
            add_list(s, BUILTIN[i].bases);
        }
    }
    return s->count;
}

/* Rank class: home 0; measured 1 (by ttfb); unmeasured 2 (list order); failed 3 (list order). */
static unsigned klass(const sel_mirror_set* s, unsigned i) {
    if (i == 0 && s->home) {
        return 0;
    }
    if (s->failures[i] > 0) {
        return 3;
    }
    return s->ttfb_ms[i] > 0 ? 1u : 2u;
}

static int before(const sel_mirror_set* s, unsigned a, unsigned b) {
    unsigned ka = klass(s, a), kb = klass(s, b);
    if (ka != kb) {
        return ka < kb;
    }
    if (ka == 1 && s->ttfb_ms[a] != s->ttfb_ms[b]) {
        return s->ttfb_ms[a] < s->ttfb_ms[b];
    }
    return a < b;
}

void sel_mirrors_rank(const sel_mirror_set* s, unsigned* order) {
    unsigned i, j;
    for (i = 0; i < s->count; i++) {
        order[i] = i;
    }
    for (i = 1; i < s->count; i++) { /* insertion sort, stable, at most 8 entries */
        unsigned v = order[i];
        for (j = i; j > 0 && before(s, v, order[j - 1]); j--) {
            order[j] = order[j - 1];
        }
        order[j] = v;
    }
}

typedef struct attempt {
    sel_mirror_sink sink;
    void* ctx;
    unsigned long start;
    unsigned ttfb;
    unsigned long bytes;
    unsigned long from;          /* resume offset of this attempt, 0 = whole body */
    sel_mirror_restart restart;
    void* rctx;
    const sel_http_res* hr;      /* final status, readable once the body flows */
    int checked;                 /* the resume answer was looked at */
} attempt;

static int on_body(void* c, const void* data, size_t len) {
    attempt* a = c;
    if (!a->checked && a->from > 0 && len > 0) {
        a->checked = 1;
        if ((a->hr == NULL || a->hr->status != 206) && a->restart != NULL) {
            a->restart(a->rctx); /* the server (or FTP) sent the whole body: start over */
        }
    }
    if (a->bytes == 0 && len > 0) {
        unsigned long d = sel_net_now_ms() - a->start;
        a->ttfb = d > 0 ? (unsigned)d : 1u;
    }
    a->bytes += len;
    return a->sink(a->ctx, data, len);
}

/* Percent-encode everything but unreserved characters and '/'. */
static int encode_path(const char* path, char* out, size_t cap) {
    size_t n = 0;
    for (; *path != '\0'; path++) {
        unsigned char c = (unsigned char)*path;
        int plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
                    c == '-' || c == '_' || c == '.' || c == '~';
        if (n + 4 > cap) {
            return 0;
        }
        if (plain) {
            out[n++] = (char)c;
        } else {
            n += (size_t)snprintf(out + n, 4, "%%%02X", c);
        }
    }
    out[n] = '\0';
    return 1;
}

static int try_one(const char* url, attempt* a, const sel_mirror_opts* o) {
    if (strncmp(url, "ftp://", 6) == 0) {
        sel_ftp_res fr;
        if (o != NULL && o->resumable != NULL) {
            *o->resumable = 0;
        }
        return sel_ftp_get(url, 0, on_body, a, &fr) == SEL_FTP_OK;
    } else {
        sel_http_req req;
        sel_http_res hr;
        memset(&req, 0, sizeof req);
        memset(&hr, 0, sizeof hr);
        req.url = url;
        if (o != NULL && o->resumable != NULL) {
            *o->resumable = 1;
        }
        if (o != NULL && o->stall_ms > 0) {
            req.connect_ms = o->stall_ms;
            req.first_byte_ms = o->stall_ms;
        }
        if (a->from > 0) {
            req.has_range = 1;
            req.range_from = a->from;
            req.range_to = -1;
        }
        a->hr = &hr;
        return sel_http_request(&req, on_body, a, &hr) == SEL_HTTP_OK && hr.status >= 200 && hr.status < 300;
    }
}

int sel_mirrors_fetch_opts(sel_mirror_set* s, const char* path, const sel_mirror_opts* o, sel_mirror_sink sink,
                           sel_mirror_restart restart, sel_mirror_verify verify, void* ctx, sel_mirror_res* res) {
    char enc[SEL_MIRROR_PATH_MAX];
    unsigned k;
    memset(res, 0, sizeof *res);
    res->served = -1;
    while (*path == '/') {
        path++;
    }
    if (s->count == 0 || !encode_path(path, enc, sizeof enc)) {
        return 0;
    }
    sel_mirrors_rank(s, res->order);
    for (k = 0; k < s->count && (o == NULL || o->max_tries == 0 || k < o->max_tries); k++) {
        unsigned i = res->order[k];
        attempt a;
        if (k > 0 && restart != NULL) {
            restart(ctx);
        }
        snprintf(res->url, sizeof res->url, "%s%s", s->base[i], enc);
        a.sink = sink;
        a.ctx = ctx;
        a.start = sel_net_now_ms();
        a.ttfb = 0;
        a.bytes = 0;
        a.from = (k == 0 && o != NULL) ? o->from : 0;
        a.restart = restart;
        a.rctx = ctx;
        a.hr = NULL;
        a.checked = 0;
        res->tries++;
        if (try_one(res->url, &a, o) && (verify == NULL || verify(ctx) || (res->rejected++, 0))) {
            if (a.ttfb > 0) {
                s->ttfb_ms[i] = a.ttfb;
            }
            s->failures[i] = 0;
            res->served = (int)i;
            res->body_bytes = a.bytes;
            res->retries = res->tries - 1u;
            return 1;
        }
        if (o != NULL && o->yielded != NULL && *o->yielded) {
            break; /* the sink stopped on purpose: not a failure of the mirror */
        }
        s->failures[i]++;
    }
    res->retries = res->tries > 0 ? res->tries - 1u : 0u;
    return 0;
}

int sel_mirrors_fetch_verified(sel_mirror_set* s, const char* path, sel_mirror_sink sink, sel_mirror_restart restart,
                               sel_mirror_verify verify, void* ctx, sel_mirror_res* res) {
    return sel_mirrors_fetch_opts(s, path, NULL, sink, restart, verify, ctx, res);
}

int sel_mirrors_fetch(sel_mirror_set* s, const char* path, sel_mirror_sink sink, sel_mirror_restart restart,
                      void* ctx, sel_mirror_res* res) {
    return sel_mirrors_fetch_verified(s, path, sink, restart, NULL, ctx, res);
}
