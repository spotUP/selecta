/* Composer name normalisation (composer.h; plan I182, T85). */
#include "composer.h"

#include <string.h>

static int is_alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80u;
}

/* Copies `name` without "(...)" groups; "Last, First" with exactly one comma becomes "First Last". Whitespace is
 * collapsed and trimmed. Case and other punctuation are kept. */
static void clean(const char* name, char* out, size_t cap) {
    char tmp[SEL_COMPOSER_MAX];
    size_t n = 0, commas = 0, comma_at = 0, i, o = 0;
    int depth = 0, space = 1;

    for (i = 0; name[i] != '\0' && n + 1 < sizeof tmp; i++) {
        unsigned char c = (unsigned char)name[i];

        if (c == '(') {
            depth++;
        } else if (c == ')') {
            if (depth > 0) depth--;
        } else if (depth == 0) {
            if (c == ',') {
                commas++;
                comma_at = n;
            }
            tmp[n++] = (char)c;
        }
    }
    tmp[n] = '\0';
    if (commas == 1) {
        char swapped[SEL_COMPOSER_MAX];

        memcpy(swapped, tmp + comma_at + 1, n - comma_at - 1u);
        swapped[n - comma_at - 1u] = ' ';
        memcpy(swapped + n - comma_at, tmp, comma_at);
        swapped[n] = '\0';
        memcpy(tmp, swapped, n + 1u);
    }
    for (i = 0; tmp[i] != '\0' && o + 1 < cap; i++) {
        char c = tmp[i];

        if (c == ' ' || c == '\t' || (c == ',' && commas == 1)) {
            if (!space) out[o++] = ' ';
            space = 1;
        } else {
            out[o++] = c;
            space = 0;
        }
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = '\0';
}

static void fold(const char* text, char* out, size_t cap) {
    size_t o = 0;
    int space = 1;

    for (; *text != '\0' && o + 1 < cap; text++) {
        unsigned char c = (unsigned char)*text;

        if (!is_alnum(c)) {
            if (!space) out[o++] = ' ';
            space = 1;
        } else {
            out[o++] = (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            space = 0;
        }
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = '\0';
}

int sel_composer_canon(const char* name, char* out, size_t cap) {
    char cleaned[SEL_COMPOSER_MAX];
    char folded[SEL_COMPOSER_MAX];

    if (out == NULL || cap == 0) return 0;
    out[0] = '\0';
    if (name == NULL) return 0;
    clean(name, cleaned, sizeof cleaned);
    fold(cleaned, folded, sizeof folded);
    strncpy(out, folded, cap - 1u);
    out[cap - 1u] = '\0';
    return out[0] != '\0';
}

int sel_composer_display(const char* name, char* out, size_t cap) {
    char cleaned[SEL_COMPOSER_MAX];
    size_t i, o = 0, start = 0;

    if (out == NULL || cap == 0) return 0;
    out[0] = '\0';
    if (name == NULL) return 0;
    clean(name, cleaned, sizeof cleaned);
    /* words of four or more letters written in capitals: "LILJEDAHL" -> "Liljedahl" */
    for (i = 0;; i++) {
        if (cleaned[i] == ' ' || cleaned[i] == '\0') {
            size_t len = i - start, k;
            int caps = len >= 4u;

            for (k = start; k < i && caps; k++) {
                if (cleaned[k] >= 'a' && cleaned[k] <= 'z') caps = 0;
            }
            for (k = start; k < i && o + 1 < cap; k++) {
                char c = cleaned[k];

                if (caps && k > start && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                out[o++] = c;
            }
            if (cleaned[i] == '\0') break;
            if (o + 1 < cap) out[o++] = ' ';
            start = i + 1u;
        }
    }
    out[o] = '\0';
    return o > 0;
}
