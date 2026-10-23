/* Library query parser and SQL compiler (plan Phase 2, I147/I148; design doc 9.2).
 * Grammar: or := and ('OR' and)*; and := unary+ (implicit AND, optional 'AND');
 * unary := ('NOT' | '-') unary | '(' or ')' | term; term := key ':' value | word.
 * SQL is emitted while parsing; every value is a bound parameter. */
#include "query.h"

#include <stddef.h>
#include <string.h>

#define VIEW_DEPTH_MAX 8

typedef enum { K_TEXT, K_INT, K_DUR, K_SIZE, K_TIME, K_BOOL, K_IN, K_VIEW } key_kind;

typedef struct key_def {
    const char* name;
    key_kind kind;
    const char* expr; /* SQL expression (text, number, time) or, for K_BOOL, "table" / "table.col = 1" */
    int never;        /* accepts the value `never` (no history rows) */
} key_def;

#define LAST_PLAYED \
    "(SELECT MAX(history.played_at) FROM history WHERE history.key = songs.key AND history.skipped = 0)"

static const key_def KEYS[] = {
    {"format", K_TEXT, "songs.format", 0},
    {"artist", K_TEXT, "songs.artist", 0},
    {"album", K_TEXT, "songs.album", 0},
    {"genre", K_TEXT, "songs.genre", 0},
    {"source", K_TEXT, "songs.source", 0},
    {"name", K_TEXT, "songs.name", 0},
    {"path", K_TEXT, "songs.path", 0},
    {"md5", K_TEXT, "songs.md5", 0},
    {"year", K_INT, "songs.year", 0},
    {"rating", K_INT, "(SELECT ratings.rating FROM ratings WHERE ratings.key = songs.key)", 0},
    {"plays", K_INT, "(SELECT COUNT(*) FROM history WHERE history.key = songs.key AND history.skipped = 0)", 0},
    {"skips", K_INT, "(SELECT COUNT(*) FROM history WHERE history.key = songs.key AND history.skipped = 1)", 0},
    {"length", K_DUR, "songs.duration_ms", 0},
    {"size", K_SIZE, "songs.size", 0},
    {"played", K_TIME, LAST_PLAYED, 1},
    {"lastplayed", K_TIME, LAST_PLAYED, 1},
    {"added", K_TIME, "songs.added", 0},
    {"liked", K_BOOL, "liked", 0},
    {"wrong", K_BOOL, "flags.wrong = 1", 0},
    {"skip", K_BOOL, "flags.skip = 1", 0},
    {"in", K_IN, NULL, 0},
    {"view", K_VIEW, NULL, 0},
};

typedef struct pq {
    sel_query* q;
    long long now;
    sel_query_view_fn views;
    void* ctx;
    char stack[VIEW_DEPTH_MAX][SEL_QUERY_TEXT_MAX]; /* view names being expanded (cycle check) */
    int depth;
    const char* p;
} pq;

static int failed(const pq* s) { return s->q->error[0] != '\0'; }

static void fail(pq* s, const char* code) {
    if (!failed(s)) {
        strncpy(s->q->error, code, sizeof s->q->error - 1);
    }
}

static void put(pq* s, const char* text) {
    size_t have = strlen(s->q->sql);
    size_t n = strlen(text);
    if (have + n + 1 > sizeof s->q->sql) {
        fail(s, "too_complex");
        return;
    }
    memcpy(s->q->sql + have, text, n + 1);
}

static sel_query_param* new_param(pq* s) {
    sel_query_param* p;
    if (s->q->nparams >= SEL_QUERY_PARAMS_MAX) {
        fail(s, "too_complex");
        return NULL;
    }
    p = &s->q->params[s->q->nparams++];
    memset(p, 0, sizeof *p);
    return p;
}

static void bind_int(pq* s, long long v) {
    sel_query_param* p = new_param(s);
    if (p != NULL) {
        p->is_int = 1;
        p->i = v;
    }
}

/* Binds text; with `like` the value is LIKE-escaped (\ % _) and wrapped in %...%. */
static void bind_text(pq* s, const char* v, size_t n, int like) {
    sel_query_param* p = new_param(s);
    size_t o = 0;
    size_t i;
    if (p == NULL) {
        return;
    }
    if (like) {
        p->s[o++] = '%';
    }
    for (i = 0; i < n; i++) {
        if (o + 4 >= sizeof p->s) {
            fail(s, "too_complex");
            return;
        }
        if (like && (v[i] == '\\' || v[i] == '%' || v[i] == '_')) {
            p->s[o++] = '\\';
        }
        p->s[o++] = v[i];
    }
    if (like) {
        p->s[o++] = '%';
    }
    p->s[o] = '\0';
}

static int is_space(char c) { return c == ' ' || c == '\t' || c == '\n'; }

static void skip_ws(pq* s) {
    while (is_space(*s->p)) {
        s->p++;
    }
}

static int word_end(char c) { return c == '\0' || is_space(c) || c == ')'; }

static int at_word(const pq* s, const char* w) {
    size_t n = strlen(w);
    return strncmp(s->p, w, n) == 0 && word_end(s->p[n]);
}

static void text_cond(pq* s, const char* col, char mode, const char* v, size_t n) {
    put(s, "COALESCE(");
    put(s, col);
    put(s, ", '')");
    if (mode == '=') {
        put(s, " = ? COLLATE NOCASE");
        bind_text(s, v, n, 0);
    } else if (mode == '/') {
        put(s, " REGEXP ?");
        bind_text(s, v, n, 0);
    } else {
        put(s, " LIKE ? ESCAPE '\\'");
        bind_text(s, v, n, 1);
    }
}

/* Unit multiplier for a number suffix; 0 when the suffix is not valid for the kind. */
static long long unit_factor(key_kind k, const char* u, size_t n) {
    char c = n == 1 ? u[0] : '\0';
    if (n > 1) {
        return 0;
    }
    if (c >= 'A' && c <= 'Z') {
        c = (char)(c - 'A' + 'a');
    }
    switch (k) {
    case K_INT:
        return n == 0 ? 1 : 0;
    case K_DUR:
        return n == 0 || c == 's' ? 1000 : c == 'm' ? 60000 : c == 'h' ? 3600000 : 0;
    case K_SIZE:
        return n == 0 || c == 'b' ? 1 : c == 'k' ? 1024 : c == 'm' ? 1048576 : c == 'g' ? 1073741824LL : 0;
    default: /* K_TIME: the unit is mandatory */
        if (n == 0) {
            return 0;
        }
        return c == 's' ? 1 : c == 'm' ? 60 : c == 'h' ? 3600 : c == 'd' ? 86400 : c == 'w' ? 604800
               : c == 'y' ? 31536000 : 0;
    }
}

static int parse_num(key_kind k, const char* v, size_t n, long long* out) {
    long long acc = 0;
    long long f;
    size_t i = 0;
    if (n == 0 || v[0] < '0' || v[0] > '9') {
        return 0;
    }
    while (i < n && v[i] >= '0' && v[i] <= '9') {
        acc = acc * 10 + (v[i] - '0');
        if (acc > 1000000000000LL) {
            return 0;
        }
        i++;
    }
    f = unit_factor(k, v + i, n - i);
    if (f == 0) {
        return 0;
    }
    *out = acc * f;
    return 1;
}

/* Numeric, duration, size and relative-time conditions: `>n`, `<n`, `=n`, `n`, `n..m`. For a time
 * key the number is an age: `<7d` means newer than seven days (value > now - 7d). */
static void num_cond(pq* s, const key_def* kd, const char* v, size_t n) {
    long long a = 0;
    long long b = 0;
    const char* dots = NULL;
    size_t i;
    char op = '=';

    for (i = 0; i + 1 < n; i++) {
        if (v[i] == '.' && v[i + 1] == '.') {
            dots = v + i;
            break;
        }
    }
    if (kd->kind == K_TIME && kd->never && n == 5 && strncmp(v, "never", 5) == 0) {
        put(s, "NOT EXISTS (SELECT 1 FROM history WHERE history.key = songs.key AND history.skipped = 0)");
        return;
    }
    if (dots != NULL) {
        if (!parse_num(kd->kind, v, (size_t)(dots - v), &a) ||
            !parse_num(kd->kind, dots + 2, n - (size_t)(dots - v) - 2, &b)) {
            fail(s, "bad_value");
            return;
        }
        if (kd->kind == K_TIME) { /* ages: between a and b ago */
            long long lo = s->now - b;
            long long hi = s->now - a;
            put(s, kd->expr);
            put(s, " BETWEEN ? AND ?");
            bind_int(s, lo);
            bind_int(s, hi);
            return;
        }
        put(s, kd->expr);
        put(s, " BETWEEN ? AND ?");
        bind_int(s, a);
        bind_int(s, b);
        return;
    }
    if (n > 0 && (v[0] == '>' || v[0] == '<' || v[0] == '=')) {
        op = v[0];
        v++;
        n--;
    }
    if (!parse_num(kd->kind, v, n, &a)) {
        fail(s, "bad_value");
        return;
    }
    put(s, kd->expr);
    if (kd->kind == K_TIME) { /* age comparison flips against the timestamp */
        put(s, op == '<' ? " > ?" : op == '>' ? " < ?" : " = ?");
        bind_int(s, s->now - a);
    } else {
        put(s, op == '<' ? " < ?" : op == '>' ? " > ?" : " = ?");
        bind_int(s, a);
    }
}

static void bool_cond(pq* s, const key_def* kd, const char* v, size_t n) {
    int yes = (n == 3 && strncmp(v, "yes", 3) == 0) || (n == 4 && strncmp(v, "true", 4) == 0) ||
              (n == 1 && v[0] == '1');
    int no = (n == 2 && strncmp(v, "no", 2) == 0) || (n == 5 && strncmp(v, "false", 5) == 0) ||
             (n == 1 && v[0] == '0');
    if (!yes && !no) {
        fail(s, "bad_value");
        return;
    }
    put(s, yes ? "EXISTS (SELECT 1 FROM " : "NOT EXISTS (SELECT 1 FROM ");
    if (strchr(kd->expr, '.') != NULL) { /* "flags.wrong = 1" */
        put(s, "flags WHERE flags.key = songs.key AND ");
        put(s, kd->expr);
    } else {
        put(s, kd->expr);
        put(s, " WHERE ");
        put(s, kd->expr);
        put(s, ".key = songs.key");
    }
    put(s, ")");
}

static void in_cond(pq* s, const char* v, size_t n) {
    put(s, "songs.key IN (SELECT playlist_entries.key FROM playlist_entries JOIN playlists ON playlists.id = "
           "playlist_entries.playlist_id LEFT JOIN folders ON folders.id = playlists.folder_id "
           "WHERE playlists.name = ? OR folders.name = ?)");
    bind_text(s, v, n, 0);
    bind_text(s, v, n, 0);
}

static void parse_or(pq* s);

static void view_cond(pq* s, const char* v, size_t n) {
    char name[SEL_QUERY_TEXT_MAX];
    const char* text;
    const char* saved;
    int i;

    if (n == 0 || n >= sizeof name) {
        fail(s, "bad_value");
        return;
    }
    memcpy(name, v, n);
    name[n] = '\0';
    for (i = 0; i < s->depth; i++) {
        if (strcmp(s->stack[i], name) == 0) {
            fail(s, "view_cycle");
            return;
        }
    }
    text = s->views != NULL ? s->views(s->ctx, name) : NULL;
    if (text == NULL) {
        fail(s, "unknown_view");
        return;
    }
    if (s->depth >= VIEW_DEPTH_MAX) {
        fail(s, "too_complex");
        return;
    }
    strcpy(s->stack[s->depth++], name);
    saved = s->p;
    s->p = text;
    put(s, "(");
    skip_ws(s);
    if (*s->p == '\0') {
        put(s, "1");
    } else {
        parse_or(s);
        if (!failed(s) && *s->p != '\0') {
            fail(s, "syntax");
        }
    }
    put(s, ")");
    s->p = saved;
    s->depth--;
}

/* One term: `key:value` or a bare word. */
static int key_eq(const char* name, const char* k, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        char c = k[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (name[i] != c) {
            return 0;
        }
    }
    return 1;
}

static void parse_term(pq* s) {
    const char* start = s->p;
    const char* k = s->p;
    const char* v;
    size_t klen = 0;
    size_t vlen;
    char mode = ' ';
    size_t i;

    s->q->terms++;
    while ((k[klen] >= 'a' && k[klen] <= 'z') || (k[klen] >= 'A' && k[klen] <= 'Z') || k[klen] == '_') {
        klen++;
    }
    if (klen > 0 && k[klen] == ':') {
        const key_def* kd = NULL;
        for (i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
            if (strlen(KEYS[i].name) == klen && key_eq(KEYS[i].name, k, klen)) {
                kd = &KEYS[i];
            }
        }
        if (kd == NULL) {
            fail(s, "unknown_key");
            return;
        }
        v = k + klen + 1;
        if (*v == '"') {
            const char* e = strchr(v + 1, '"');
            if (e == NULL) {
                fail(s, "syntax");
                return;
            }
            vlen = (size_t)(e - v - 1);
            v++;
            s->p = e + 1;
        } else if (*v == '/' && kd->kind == K_TEXT) {
            const char* e = v + 1;
            while (*e != '\0' && !(*e == '/' && word_end(e[1]))) {
                e++;
            }
            if (*e == '\0') {
                fail(s, "syntax");
                return;
            }
            mode = '/';
            vlen = (size_t)(e - v - 1);
            v++;
            s->p = e + 1;
        } else {
            vlen = 0;
            while (!word_end(v[vlen])) {
                vlen++;
            }
            s->p = v + vlen;
            if (kd->kind == K_TEXT && vlen > 0 && v[0] == '=') {
                mode = '=';
                v++;
                vlen--;
            }
        }
        switch (kd->kind) {
        case K_TEXT:
            text_cond(s, kd->expr, mode, v, vlen);
            break;
        case K_BOOL:
            bool_cond(s, kd, v, vlen);
            break;
        case K_IN:
            in_cond(s, v, vlen);
            break;
        case K_VIEW:
            view_cond(s, v, vlen);
            break;
        default:
            num_cond(s, kd, v, vlen);
            break;
        }
        return;
    }
    /* bare word or quoted phrase: matches name, artist, path */
    if (*start == '"') {
        const char* e = strchr(start + 1, '"');
        if (e == NULL) {
            fail(s, "syntax");
            return;
        }
        v = start + 1;
        vlen = (size_t)(e - v);
        s->p = e + 1;
    } else {
        v = start;
        vlen = 0;
        while (!word_end(v[vlen])) {
            vlen++;
        }
        s->p = v + vlen;
    }
    put(s, "(");
    text_cond(s, "songs.name", ' ', v, vlen);
    put(s, " OR ");
    text_cond(s, "songs.artist", ' ', v, vlen);
    put(s, " OR ");
    text_cond(s, "songs.path", ' ', v, vlen);
    put(s, ")");
}

static void parse_unary(pq* s) {
    skip_ws(s);
    if (failed(s)) {
        return;
    }
    if (at_word(s, "NOT")) {
        s->p += 3;
        put(s, "NOT ");
        skip_ws(s);
        if (*s->p == '\0' || *s->p == ')') {
            fail(s, "syntax");
            return;
        }
        parse_unary(s);
    } else if (*s->p == '-' && !is_space(s->p[1]) && s->p[1] != '\0') {
        s->p++;
        put(s, "NOT ");
        parse_unary(s);
    } else if (*s->p == '(') {
        s->p++;
        put(s, "(");
        parse_or(s);
        skip_ws(s);
        if (*s->p != ')') {
            fail(s, "syntax");
            return;
        }
        s->p++;
        put(s, ")");
    } else if (*s->p == '\0' || *s->p == ')') {
        fail(s, "syntax");
    } else {
        parse_term(s);
    }
}

static void parse_and(pq* s) {
    parse_unary(s);
    for (;;) {
        skip_ws(s);
        if (failed(s) || *s->p == '\0' || *s->p == ')' || at_word(s, "OR")) {
            return;
        }
        if (at_word(s, "AND")) {
            s->p += 3;
        }
        put(s, " AND ");
        parse_unary(s);
    }
}

static void parse_or(pq* s) {
    parse_and(s);
    for (;;) {
        skip_ws(s);
        if (failed(s) || !at_word(s, "OR")) {
            return;
        }
        s->p += 2;
        put(s, " OR ");
        parse_and(s);
    }
}

int sel_query_compile(const char* text, long long now, sel_query_view_fn views, void* ctx, sel_query* out) {
    static const char PREFIX[] = "SELECT songs.key FROM songs WHERE ";
    pq s;

    memset(out, 0, sizeof *out);
    memset(&s, 0, sizeof s);
    s.q = out;
    s.now = now;
    s.views = views;
    s.ctx = ctx;
    s.p = text != NULL ? text : "";
    memcpy(out->sql, PREFIX, sizeof PREFIX);
    skip_ws(&s);
    if (*s.p == '\0') {
        put(&s, "1");
        return 1;
    }
    parse_or(&s);
    skip_ws(&s);
    if (!failed(&s) && *s.p != '\0') {
        fail(&s, "syntax"); /* stray ')' */
    }
    return !failed(&s);
}
