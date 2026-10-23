/* Small regular-expression matcher behind the query language's `key:/re/` (plan Phase 2, I149).
 * Backtracking, case-insensitive, searches anywhere in the text. Supported: literals, `.`, classes
 * `[a-z0-9_]` and `[^...]`, escapes `\x`, anchors `^` and `$`, and the quantifiers `*`, `+`, `?`
 * after a single atom. No groups, no alternation (deviation, documented in the Phase 2c report).
 * No allocation and no recursion deeper than the pattern length, so it runs on the 68k as well. */
#include "regex.h"

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

/* One atom at p: returns its length in the pattern, 0 when malformed. */
static int atom_len(const char* p) {
    int i;
    if (p[0] == '\\') {
        return p[1] != '\0' ? 2 : 0;
    }
    if (p[0] == '[') {
        i = 1;
        if (p[i] == '^') {
            i++;
        }
        if (p[i] == ']') {
            i++;
        }
        while (p[i] != '\0' && p[i] != ']') {
            i++;
        }
        return p[i] == ']' ? i + 1 : 0;
    }
    return p[0] != '\0' ? 1 : 0;
}

static int atom_match(const char* p, int len, int c) {
    int i;
    int neg = 0;
    int hit = 0;
    c = lower(c);
    if (p[0] == '\\') {
        return lower(p[1]) == c;
    }
    if (p[0] == '.') {
        return 1;
    }
    if (p[0] != '[') {
        return lower(p[0]) == c;
    }
    i = 1;
    if (p[i] == '^') {
        neg = 1;
        i++;
    }
    for (; i < len - 1; i++) {
        int lo = lower((unsigned char)p[i]);
        if (p[i] == '\\' && i + 1 < len - 1) {
            lo = lower((unsigned char)p[++i]);
        }
        if (p[i + 1] == '-' && i + 2 < len - 1) {
            int hi = lower((unsigned char)p[i + 2]);
            if (c >= lo && c <= hi) {
                hit = 1;
            }
            i += 2;
        } else if (c == lo) {
            hit = 1;
        }
    }
    return hit != neg;
}

/* Matches pattern p against text t from t's current position; -1 on a malformed pattern. */
static int match_here(const char* p, const char* t) {
    int len;
    char q;
    if (p[0] == '\0') {
        return 1;
    }
    if (p[0] == '$' && p[1] == '\0') {
        return *t == '\0';
    }
    len = atom_len(p);
    if (len == 0) {
        return -1;
    }
    q = p[len];
    if (q == '*' || q == '+' || q == '?') {
        const char* rest = p + len + 1;
        const char* e = t;
        int min = q == '+' ? 1 : 0;
        int max = q == '?' ? 1 : 0x7fffffff;
        int n = 0;
        while (n < max && *e != '\0' && atom_match(p, len, (unsigned char)*e)) {
            e++;
            n++;
        }
        for (; n >= min; n--, e--) {
            int r = match_here(rest, e);
            if (r != 0) {
                return r;
            }
        }
        return 0;
    }
    if (*t != '\0' && atom_match(p, len, (unsigned char)*t)) {
        return match_here(p + len, t + 1);
    }
    return 0;
}

int sel_regex_match(const char* pattern, const char* text) {
    if (pattern[0] == '^') {
        return match_here(pattern + 1, text);
    }
    do {
        int r = match_here(pattern, text);
        if (r != 0) {
            return r;
        }
    } while (*text++ != '\0');
    return 0;
}
