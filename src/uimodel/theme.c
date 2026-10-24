/* Text UI theme tokens, plan M1.1 phase 5 (row I59). */
#include <selecta/uimodel/theme.h>

#include <string.h>

static const char* const role_names[TXT_ROLE_COUNT] = {
#define X(id, name) name,
    TXT_THEME_ROLES(X)
#undef X
};

const char* txt_role_name(txt_role r) { return (unsigned)r < TXT_ROLE_COUNT ? role_names[r] : ""; }

void txt_theme_default(txt_theme* t) {
    const uint16_t D = TXT_COLOR_DEFAULT;
    uint32_t i;
    t->role[TXT_ROLE_NORMAL] = txt_style_make(252, D, 0);
    t->role[TXT_ROLE_HEADER] = txt_style_make(231, 24, TXT_ATTR_BOLD);
    t->role[TXT_ROLE_CURSOR] = txt_style_make(16, 110, 0);
    t->role[TXT_ROLE_DIRECTORY] = txt_style_make(75, D, TXT_ATTR_BOLD);
    t->role[TXT_ROLE_FILE] = txt_style_make(252, D, 0);
    t->role[TXT_ROLE_PLAYING] = txt_style_make(114, D, TXT_ATTR_BOLD);
    t->role[TXT_ROLE_DIM] = txt_style_make(244, D, 0);
    t->role[TXT_ROLE_LABEL] = txt_style_make(109, D, 0);
    t->role[TXT_ROLE_VALUE] = txt_style_make(255, D, 0);
    t->role[TXT_ROLE_BORDER] = txt_style_make(240, D, 0);
    t->role[TXT_ROLE_ERROR] = txt_style_make(203, D, TXT_ATTR_BOLD);
    for (i = 0; i < SEL_GUI_BASE_COUNT; i++) {
        t->base[i] = sel_gui_base_default((sel_gui_base)i);
        t->base_ref[i] = -1;
    }
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        t->token[i] = sel_gui_token_default((sel_gui_token)i);
        t->ref[i] = (int8_t)sel_gui_token_default_ref((sel_gui_token)i);
    }
    t->bad_token[0] = '\0';
    t->bad_range = 0;
}

static int fail_token(txt_theme* t, const char* name, size_t n) {
    if (n >= sizeof t->bad_token) n = sizeof t->bad_token - 1u;
    memcpy(t->bad_token, name, n);
    t->bad_token[n] = '\0';
    return 0;
}

/* A resolved value v outside [lo, hi] (a cycle is v < 0 and stays a reference failure): names the token, flags range. */
static int fail_value(txt_theme* t, const char* key, int64_t v) {
    fail_token(t, key, strlen(key));
    t->bad_range = v >= 0;
    return 0;
}

static size_t word(const char** p, const char* end, const char** w);

/* Value of base token b after following its references; -1 on a cycle (more hops than base tokens). */
static int64_t base_value(const txt_theme* t, int b) {
    int hops;
    for (hops = 0; hops <= (int)SEL_GUI_BASE_COUNT; hops++) {
        if (t->base_ref[b] < 0) return (int64_t)t->base[b];
        b = t->base_ref[b];
    }
    return -1;
}

/* Sets a role (is_base 0) or base token to `@name`; fails naming `name` (missing) or the token (cycle, range). */
static int apply_ref(txt_theme* t, int tok, int is_base, const char* w, size_t n) {
    int b = sel_gui_base_find(w + 1, (uint32_t)(n - 1u));
    int64_t v;
    uint32_t lo, hi;
    const char* key = is_base ? sel_gui_base_name((sel_gui_base)tok) : sel_gui_token_name((sel_gui_token)tok);
    if (b < 0) return fail_token(t, w + 1, n - 1u);
    if (is_base) {
        int8_t keep = t->base_ref[tok];
        t->base_ref[tok] = (int8_t)b;
        v = base_value(t, tok);
        lo = sel_gui_base_min((sel_gui_base)tok);
        hi = sel_gui_base_max((sel_gui_base)tok);
        if (v < (int64_t)lo || v > (int64_t)hi) {
            t->base_ref[tok] = keep;
            return fail_value(t, key, v);
        }
        return 1;
    }
    v = base_value(t, b);
    lo = sel_gui_token_min((sel_gui_token)tok);
    hi = sel_gui_token_max((sel_gui_token)tok);
    if (v < (int64_t)lo || v > (int64_t)hi) return fail_value(t, key, v);
    t->ref[tok] = (int8_t)b;
    t->token[tok] = (uint32_t)v;
    return 1;
}

/* `value` is decimal or 0x hex or @gui.base.<name>, nothing else on the line, and inside the token's range. */
static int apply_token(txt_theme* t, int tok, int is_base, const char* s, const char* end) {
    const char* w;
    size_t n = word(&s, end, &w), i = 0;
    uint32_t base = 10u, v = 0u;
    if (n > 1 && w[0] == '@') {
        const char* w2;
        if (word(&s, end, &w2) != 0) return 0;
        return apply_ref(t, tok, is_base, w, n);
    }
    if (n > 2 && w[0] == '0' && (w[1] == 'x' || w[1] == 'X')) {
        base = 16u;
        i = 2;
    }
    if (n == i) return 0;
    for (; i < n; i++) {
        uint32_t d;
        if (w[i] >= '0' && w[i] <= '9') d = (uint32_t)(w[i] - '0');
        else if (base == 16u && w[i] >= 'a' && w[i] <= 'f') d = (uint32_t)(w[i] - 'a') + 10u;
        else if (base == 16u && w[i] >= 'A' && w[i] <= 'F') d = (uint32_t)(w[i] - 'A') + 10u;
        else return 0;
        if (v > (0xFFFFFFFFu - d) / base) return 0;
        v = v * base + d;
    }
    if (word(&s, end, &w) != 0) return 0;
    if (is_base) {
        if (v < sel_gui_base_min((sel_gui_base)tok) || v > sel_gui_base_max((sel_gui_base)tok)) return 0;
        t->base[tok] = v;
        t->base_ref[tok] = -1;
        return 1;
    }
    if (v < sel_gui_token_min((sel_gui_token)tok) || v > sel_gui_token_max((sel_gui_token)tok)) return 0;
    t->token[tok] = v;
    t->ref[tok] = -1;
    return 1;
}

static int parse_color(const char* s, size_t n, uint16_t* out) {
    unsigned v = 0;
    size_t i;
    if (n == 7 && memcmp(s, "default", 7) == 0) {
        *out = TXT_COLOR_DEFAULT;
        return 1;
    }
    if (n == 0 || n > 3) return 0;
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10u + (unsigned)(s[i] - '0');
    }
    if (v > 255u) return 0;
    *out = (uint16_t)v;
    return 1;
}

/* Splits the next blank-separated word; returns its length (0 = end). */
static size_t word(const char** p, const char* end, const char** w) {
    const char* s = *p;
    while (s < end && (*s == ' ' || *s == '\t')) s++;
    *w = s;
    while (s < end && *s != ' ' && *s != '\t') s++;
    *p = s;
    return (size_t)(s - *w);
}

static int apply_line(txt_theme* t, const char* s, const char* end) {
    const char *w, *eq, *rp;
    size_t n, nrole;
    int r;
    txt_style st;
    for (eq = s; eq < end && *eq != '='; eq++) {
    }
    if (eq == end) return 0;
    rp = s;
    nrole = word(&rp, eq, &w);
    if (nrole == 0) return 0;
    for (r = 0; r < TXT_ROLE_COUNT; r++)
        if (strlen(role_names[r]) == nrole && memcmp(role_names[r], w, nrole) == 0) break;
    if (r == TXT_ROLE_COUNT) {
        int is_base = 0, tok = sel_gui_token_find(w, (uint32_t)nrole);
        if (tok < 0) {
            tok = sel_gui_base_find(w, (uint32_t)nrole);
            is_base = 1;
        }
        if (tok < 0 || word(&rp, eq, &w) != 0) return 0;
        return apply_token(t, tok, is_base, eq + 1, end);
    }
    if (word(&rp, eq, &w) != 0) return 0; /* junk before '=' */
    s = eq + 1;
    st.attr = 0;
    n = word(&s, end, &w);
    if (!parse_color(w, n, &st.fg)) return 0;
    n = word(&s, end, &w);
    if (!parse_color(w, n, &st.bg)) return 0;
    while ((n = word(&s, end, &w)) != 0) {
        if (n == 4 && memcmp(w, "bold", 4) == 0) st.attr |= TXT_ATTR_BOLD;
        else if (n == 7 && memcmp(w, "reverse", 7) == 0) st.attr |= TXT_ATTR_REVERSE;
        else return 0;
    }
    t->role[r] = st;
    return 1;
}

/* Resolves every role that follows a base token (once per parse, never per frame). */
static int resolve_roles(txt_theme* t) {
    int i;
    for (i = 0; i < (int)SEL_GUI_TOKEN_COUNT; i++) {
        int64_t v;
        if (t->ref[i] < 0) continue;
        v = base_value(t, t->ref[i]);
        if (v < (int64_t)sel_gui_token_min((sel_gui_token)i) || v > (int64_t)sel_gui_token_max((sel_gui_token)i)) {
            return fail_value(t, sel_gui_token_name((sel_gui_token)i), v);
        }
        t->token[i] = (uint32_t)v;
    }
    return 1;
}

int txt_theme_parse(txt_theme* t, const char* text, int* bad_line) {
    int line = 0, applied = 0;
    t->bad_token[0] = '\0';
    t->bad_range = 0;
    const char* p = text;
    while (*p) {
        const char* e = p;
        const char* hash;
        const char* c;
        int blank = 1;
        line++;
        while (*e && *e != '\n') e++;
        for (hash = p; hash < e && *hash != '#'; hash++) {
        }
        for (c = p; c < hash; c++)
            if (*c != ' ' && *c != '\t' && *c != '\r') blank = 0;
        if (!blank) {
            const char* le = hash;
            if (le > p && le[-1] == '\r') le--;
            if (!apply_line(t, p, le)) {
                if (bad_line) *bad_line = line;
                return -1;
            }
            applied++;
        }
        p = *e ? e + 1 : e;
    }
    if (!resolve_roles(t)) {
        if (bad_line) *bad_line = line;
        return -1;
    }
    return applied;
}
