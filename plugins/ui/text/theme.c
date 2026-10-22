/* Text UI theme tokens, plan M1.1 phase 5 (row I59). */
#include "theme.h"

#include <string.h>

static const char* const role_names[TXT_ROLE_COUNT] = {
#define X(id, name) name,
    TXT_THEME_ROLES(X)
#undef X
};

const char* txt_role_name(txt_role r) { return (unsigned)r < TXT_ROLE_COUNT ? role_names[r] : ""; }

void txt_theme_default(txt_theme* t) {
    const uint16_t D = TXT_COLOR_DEFAULT;
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
    if (r == TXT_ROLE_COUNT) return 0;
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

int txt_theme_parse(txt_theme* t, const char* text, int* bad_line) {
    int line = 0, applied = 0;
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
    return applied;
}
