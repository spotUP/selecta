/* I1387: see include/selecta/uimodel/title.h. */
#include <selecta/uimodel/title.h>

#include <stdio.h>
#include <string.h>

static int is_up(char c) { return c >= 'A' && c <= 'Z'; }
static int is_lo(char c) { return c >= 'a' && c <= 'z'; }

/* 1 when w[i] (0 < i < n) starts a new hump: lower then upper, and not the "Mc" / "Mac" prefix of a name. */
static int hump(const char* w, size_t i) {
    return is_lo(w[i - 1]) && is_up(w[i]) && !(i == 2 && w[0] == 'M' && w[1] == 'c') && !(i == 3 && w[0] == 'M' && w[1] == 'a' && w[2] == 'c');
}

void sel_uim_tidy_title(const char* in, char* out, size_t cap) {
    size_t o = 0, w = 0, i, n;
    int had_space = 0, split = 0, pending = 0, full = 0;
    if (cap == 0u) {
        return;
    }
    if (in == NULL) {
        in = "";
    }
    for (i = 0; in[i] != '\0'; i++) {
        had_space |= in[i] == ' ';
    }
    for (i = 0; in[i] != '\0' && !full; i++) {
        char c = in[i] == '_' ? ' ' : in[i];
        if (c == ' ') {
            pending = o > 0u; /* collapse and trim: a blank is written only before the next character */
            w = 0;
            continue;
        }
        if (w == 0u) { /* a word starts: decide once whether it is split */
            size_t humps = 0;
            for (n = 1; in[i + n] != '\0' && in[i + n] != ' ' && in[i + n] != '_'; n++) {
                humps += (size_t)hump(in + i, n);
            }
            split = !had_space && is_up(c) && humps >= 2u;
        }
        if (pending || (split && w > 0u && hump(in + i - w, w))) {
            if (o + 1u >= cap) {
                full = 1;
                break;
            }
            out[o++] = ' ';
            pending = 0;
        }
        if (o + 1u >= cap) {
            full = 1;
            break;
        }
        out[o++] = c;
        w++;
    }
    if (full && (((unsigned char)in[i]) & 0xC0u) == 0x80u) { /* never leave half a UTF-8 sequence */
        while (o > 0u && (((unsigned char)out[o - 1u]) & 0xC0u) == 0x80u) {
            o--;
        }
        if (o > 0u) {
            o--;
        }
    }
    out[o] = '\0';
    for (i = 0; i < o && !is_up(out[i]); i++) {
    }
    if (i == o) { /* no capital at all: capitalise each word */
        for (i = 0; i < o; i++) {
            if (is_lo(out[i]) && (i == 0u || out[i - 1u] == ' ')) {
                out[i] = (char)(out[i] - 'a' + 'A');
            }
        }
    }
}

/* I1387b: a Song Information value that is a name (labels "Title", "Author", "Release", "STIL title") is tidied in place; other rows stay. */
void sel_uim_tidy_info(const char* label, char* val, size_t cap) {
    char tidy[256];
    if (label == NULL || val == NULL || cap == 0u || strlen(val) >= sizeof tidy / 2u) {
        return;
    }
    if (strcmp(label, "Title") != 0 && strcmp(label, "Author") != 0 && strcmp(label, "Release") != 0 && strcmp(label, "STIL title") != 0) {
        return;
    }
    sel_uim_tidy_title(val, tidy, sizeof tidy);
    snprintf(val, cap, "%s", tidy);
}
