/* See json_scan.h. */
#include "json_scan.h"

static int utf8_put(char* out, size_t n, size_t* used, unsigned long cp) {
    char b[4] = {0, 0, 0, 0}; /* GCC -O3 cannot see that only the first len bytes are read */
    size_t len, i;

    if (cp < 0x80u) {
        b[0] = (char)cp;
        len = 1;
    } else if (cp < 0x800u) {
        b[0] = (char)(0xC0u | (cp >> 6));
        b[1] = (char)(0x80u | (cp & 0x3Fu));
        len = 2;
    } else if (cp < 0x10000u) {
        b[0] = (char)(0xE0u | (cp >> 12));
        b[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (char)(0x80u | (cp & 0x3Fu));
        len = 3;
    } else {
        b[0] = (char)(0xF0u | (cp >> 18));
        b[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (char)(0x80u | (cp & 0x3Fu));
        len = 4;
    }
    if (out == NULL || *used + len + 1u > n) {
        return 0; /* cut: the rest of the string is read and dropped */
    }
    for (i = 0; i < len; i++) {
        out[(*used)++] = b[i];
    }
    return 1;
}

static int hex4(FILE* f, unsigned long* v) {
    int i;

    *v = 0;
    for (i = 0; i < 4; i++) {
        int c = getc(f);

        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 10;
        } else {
            return 0;
        }
        *v = *v * 16u + (unsigned long)c;
    }
    return 1;
}

/* Reads a string whose opening quote is consumed. Stores it (cut to `n` - 1 bytes) in `out` when not NULL.
 * Returns 0 on a malformed string or end of file. */
int sel_json_read_string(FILE* f, char* out, size_t n) {
    size_t used = 0;
    int c;

    if (out != NULL && n > 0) {
        out[0] = '\0';
    }
    while ((c = getc(f)) != EOF) {
        if (c == '"') {
            if (out != NULL && n > 0) {
                out[used] = '\0';
            }
            return 1;
        }
        if (c == '\\') {
            unsigned long cp;

            c = getc(f);
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u':
                if (!hex4(f, &cp)) {
                    return 0;
                }
                if (cp >= 0xD800u && cp < 0xDC00u) { /* high surrogate: the low one follows as \uXXXX */
                    unsigned long lo;

                    if (getc(f) != '\\' || getc(f) != 'u' || !hex4(f, &lo)) {
                        return 0;
                    }
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                }
                (void)utf8_put(out, n, &used, cp);
                continue;
            case EOF: return 0;
            default: break; /* \" \\ \/ stand for themselves */
            }
        }
        if (out != NULL && used + 2u <= n) {
            out[used++] = (char)c;
        }
    }
    return 0;
}

int sel_json_skip_ws(FILE* f) {
    int c;

    do {
        c = getc(f);
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    return c;
}

/* Skips a value whose first byte `c` is read: a string, number or literal, or a nested object or array. Leaves the
 * delimiter that ends it (',' '}' ']') in `*end`. */
int sel_json_skip_value(FILE* f, int c, int* end) {
    int depth = 0;

    for (; c != EOF; c = getc(f)) {
        if (c == '"') {
            if (!sel_json_read_string(f, NULL, 0)) {
                return 0;
            }
        } else if (c == '{' || c == '[') {
            depth++;
        } else if (c == '}' || c == ']') {
            if (depth == 0) {
                *end = c;
                return 1;
            }
            depth--;
        } else if (c == ',' && depth == 0) {
            *end = c;
            return 1;
        }
    }
    return 0;
}
