/* Share links: generate and parse (plan M3 Phase 7, I180, D-M3-11). See share.h for the forms. */
#include "share.h"
#include "source.h"

#include <stdio.h>
#include <string.h>

#define SELECTA_PREFIX "selecta://"

/* Bounded output cursor: every put fails once the buffer is full, so a caller checks `ok` once. */
typedef struct wbuf {
    char* p;
    size_t n, len;
    int ok;
} wbuf;

static void w_init(wbuf* w, char* out, size_t n) {
    w->p = out;
    w->n = n;
    w->len = 0;
    w->ok = 1;
    out[0] = '\0';
}

static void w_char(wbuf* w, char c) {
    if (w->len + 1u >= w->n) {
        w->ok = 0;
        return;
    }
    w->p[w->len++] = c;
    w->p[w->len] = '\0';
}

static void w_str(wbuf* w, const char* s) {
    for (; *s != '\0'; s++) w_char(w, *s);
}

/* Percent-encodes everything but unreserved characters (and '/' when keep_slash). */
static void w_enc(wbuf* w, const char* s, int keep_slash) {
    static const char hex[] = "0123456789ABCDEF";
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char)*s;
        int plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                    c == '_' || c == '~' || (keep_slash && c == '/');
        if (plain) {
            w_char(w, (char)c);
        } else {
            w_char(w, '%');
            w_char(w, hex[c >> 4]);
            w_char(w, hex[c & 15u]);
        }
    }
}

static void w_uint(wbuf* w, uint32_t v) {
    char t[12];
    snprintf(t, sizeof t, "%u", (unsigned)v);
    w_str(w, t);
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decodes text[0..len) into out (NUL-terminated). 0 on a bad escape, an embedded NUL or overflow. */
static int decode(const char* text, size_t len, char* out, size_t n) {
    size_t i, o = 0;
    for (i = 0; i < len; i++) {
        char c = text[i];
        if (c == '%') {
            int hi, lo;
            if (i + 2u >= len) return 0;
            hi = hexval(text[i + 1u]);
            lo = hexval(text[i + 2u]);
            if (hi < 0 || lo < 0 || (hi == 0 && lo == 0)) return 0;
            c = (char)(hi * 16 + lo);
            i += 2u;
        }
        if (o + 1u >= n) return 0;
        out[o++] = c;
    }
    out[o] = '\0';
    return 1;
}

static int parse_uint(const char* s, size_t len, uint32_t* out) {
    uint64_t v = 0;
    size_t i;
    if (len == 0 || len > 9u) return 0;
    for (i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10u + (uint64_t)(s[i] - '0');
    }
    *out = (uint32_t)v;
    return 1;
}

int sel_share_is_link(const char* text) {
    return text != NULL && (strncmp(text, SELECTA_PREFIX, strlen(SELECTA_PREFIX)) == 0 ||
                            strncmp(text, SEL_SHARE_DEVILBOX_BASE, strlen(SEL_SHARE_DEVILBOX_BASE)) == 0);
}

/* "<source>/<rest>" (already decoded) into link->source and link->path. */
static int split_source(const char* s, sel_share_link* out) {
    const char* slash = strchr(s, '/');
    size_t sl;
    if (slash == NULL || slash == s) return 0;
    sl = (size_t)(slash - s);
    if (sl >= sizeof out->source || slash[1] == '\0') return 0;
    memcpy(out->source, s, sl);
    out->source[sl] = '\0';
    if (strlen(slash + 1) >= sizeof out->path) return 0;
    strcpy(out->path, slash + 1);
    return 1;
}

/* Walks "a=b&c=d" pairs; sub and t fill the link, other names are ignored. `want` is the DEViLBOX target
 * parameter ("song", "playlist", "view") or NULL for the selecta:// form; its decoded value goes to `target`. */
static int parse_params(const char* q, size_t qlen, sel_share_link* out, const char* want, char* target, size_t tn,
                        int* found) {
    size_t pos = 0;
    while (pos < qlen) {
        const char* amp = memchr(q + pos, '&', qlen - pos);
        size_t end = amp != NULL ? (size_t)(amp - q) : qlen;
        const char* eq = memchr(q + pos, '=', end - pos);
        if (eq != NULL) {
            size_t kl = (size_t)(eq - (q + pos));
            const char* v = eq + 1;
            size_t vl = end - (size_t)(v - q);
            if (kl == 3u && memcmp(q + pos, "sub", 3) == 0) {
                if (!parse_uint(v, vl, &out->sub)) return 0;
                out->has_sub = 1;
            } else if (kl == 1u && q[pos] == 't') {
                if (!parse_uint(v, vl, &out->t_ms)) return 0;
                out->has_t = 1;
            } else if (want != NULL && kl == strlen(want) && memcmp(q + pos, want, kl) == 0) {
                if (!decode(v, vl, target, tn)) return 0;
                *found = 1;
            }
        }
        pos = end + 1u;
    }
    return 1;
}

static int parse_selecta(const char* text, sel_share_link* out) {
    const char* rest = text + strlen(SELECTA_PREFIX);
    const char* q = strchr(rest, '?');
    const char* slash = strchr(rest, '/');
    size_t body_len = q != NULL ? (size_t)(q - rest) : strlen(rest);
    char body[SEL_SHARE_PATH_MAX + SEL_SHARE_SOURCE_MAX + 8u];
    char kind[12];
    size_t kl;
    int dummy = 0;
    if (slash == NULL || slash == rest || (q != NULL && slash > q)) return 0;
    kl = (size_t)(slash - rest);
    if (kl >= sizeof kind) return 0;
    memcpy(kind, rest, kl);
    kind[kl] = '\0';
    if (!decode(slash + 1, (size_t)((rest + body_len) - (slash + 1)), body, sizeof body)) return 0;
    if (strcmp(kind, "view") == 0) {
        if (q != NULL || body[0] == '\0' || strlen(body) >= SEL_SHARE_QUERY_MAX) return 0;
        out->kind = SEL_SHARE_VIEW;
        strcpy(out->path, body);
        return 1;
    }
    if (!split_source(body, out)) return 0;
    if (strcmp(kind, "song") == 0) {
        out->kind = SEL_SHARE_SONG;
        if (q != NULL && !parse_params(q + 1, strlen(q + 1), out, NULL, NULL, 0, &dummy)) return 0;
        return 1;
    }
    if (strcmp(kind, "playlist") == 0) {
        out->kind = SEL_SHARE_PLAYLIST;
        return q == NULL;
    }
    return 0;
}

static int parse_devilbox(const char* text, sel_share_link* out) {
    const char* q = text + strlen(SEL_SHARE_DEVILBOX_BASE);
    char target[SEL_SHARE_PATH_MAX + SEL_SHARE_SOURCE_MAX + 8u];
    static const char* const names[] = {"song", "playlist", "view"};
    static const sel_share_kind kinds[] = {SEL_SHARE_SONG, SEL_SHARE_PLAYLIST, SEL_SHARE_VIEW};
    unsigned i;
    if (*q != '?') return 0;
    q++;
    for (i = 0; i < 3u; i++) {
        int found = 0;
        sel_share_link tmp;
        memset(&tmp, 0, sizeof tmp);
        if (!parse_params(q, strlen(q), &tmp, names[i], target, sizeof target, &found)) return 0;
        if (!found) continue;
        if (kinds[i] == SEL_SHARE_VIEW) {
            if (target[0] == '\0' || strlen(target) >= SEL_SHARE_QUERY_MAX || tmp.has_sub || tmp.has_t) return 0;
            tmp.kind = SEL_SHARE_VIEW;
            strcpy(tmp.path, target);
        } else {
            if (!split_source(target, &tmp)) return 0;
            tmp.kind = kinds[i];
            if (kinds[i] == SEL_SHARE_PLAYLIST && (tmp.has_sub || tmp.has_t)) return 0;
        }
        *out = tmp;
        return 1;
    }
    return 0;
}

int sel_share_parse(const char* text, sel_share_link* out) {
    if (text == NULL || out == NULL) return 0;
    memset(out, 0, sizeof *out);
    if (strncmp(text, SELECTA_PREFIX, strlen(SELECTA_PREFIX)) == 0) {
        if (parse_selecta(text, out)) return 1;
    } else if (strncmp(text, SEL_SHARE_DEVILBOX_BASE, strlen(SEL_SHARE_DEVILBOX_BASE)) == 0) {
        if (parse_devilbox(text, out)) return 1;
    }
    memset(out, 0, sizeof *out);
    return 0;
}

static int complete(const sel_share_link* l) {
    if (l == NULL) return 0;
    if (l->kind == SEL_SHARE_VIEW) return l->path[0] != '\0';
    if (l->kind == SEL_SHARE_SONG || l->kind == SEL_SHARE_PLAYLIST) return l->source[0] != '\0' && l->path[0] != '\0';
    return 0;
}

int sel_share_format(const sel_share_link* l, char* out, size_t n) {
    wbuf w;
    if (!complete(l) || out == NULL || n == 0) return 0;
    w_init(&w, out, n);
    w_str(&w, SELECTA_PREFIX);
    if (l->kind == SEL_SHARE_VIEW) {
        w_str(&w, "view/");
        w_enc(&w, l->path, 0);
        return w.ok;
    }
    w_str(&w, l->kind == SEL_SHARE_SONG ? "song/" : "playlist/");
    w_enc(&w, l->source, 0);
    w_char(&w, '/');
    w_enc(&w, l->path, 1);
    if (l->kind == SEL_SHARE_SONG && (l->has_sub || l->has_t)) {
        w_char(&w, '?');
        if (l->has_sub) {
            w_str(&w, "sub=");
            w_uint(&w, l->sub);
        }
        if (l->has_t) {
            if (l->has_sub) w_char(&w, '&');
            w_str(&w, "t=");
            w_uint(&w, l->t_ms);
        }
    }
    return w.ok;
}

int sel_share_format_devilbox(const sel_share_link* l, char* out, size_t n) {
    wbuf w;
    if (!complete(l) || out == NULL || n == 0) return 0;
    w_init(&w, out, n);
    w_str(&w, SEL_SHARE_DEVILBOX_BASE);
    w_char(&w, '?');
    if (l->kind == SEL_SHARE_VIEW) {
        w_str(&w, "view=");
        w_enc(&w, l->path, 0);
        return w.ok;
    }
    w_str(&w, l->kind == SEL_SHARE_SONG ? "song=" : "playlist=");
    w_enc(&w, l->source, 0);
    w_str(&w, "%2F");
    w_enc(&w, l->path, 0);
    if (l->kind == SEL_SHARE_SONG && l->has_sub) {
        w_str(&w, "&sub=");
        w_uint(&w, l->sub);
    }
    if (l->kind == SEL_SHARE_SONG && l->has_t) {
        w_str(&w, "&t=");
        w_uint(&w, l->t_ms);
    }
    return w.ok;
}

int sel_share_song_open_path(const sel_share_link* l, char* out, size_t n) {
    if (l == NULL || l->kind != SEL_SHARE_SONG || out == NULL) return 0;
    if (strcmp(l->source, "local") == 0) {
        if (strlen(l->path) >= n) return 0;
        strcpy(out, l->path);
        return 1;
    }
    return sel_song_key(l->source, l->path, out, n);
}

int sel_share_equal(const sel_share_link* a, const sel_share_link* b) {
    return a->kind == b->kind && strcmp(a->source, b->source) == 0 && strcmp(a->path, b->path) == 0 &&
           a->has_sub == b->has_sub && (!a->has_sub || a->sub == b->sub) && a->has_t == b->has_t &&
           (!a->has_t || a->t_ms == b->t_ms);
}

int sel_share_key(const char* source, const char* path, char* out, size_t n) {
    return sel_song_key(source, path, out, n);
}

int sel_share_key_split(const char* key, char* source, size_t sn, char* path, size_t pn) {
    return sel_song_key_split(key, source, sn, path, pn);
}
