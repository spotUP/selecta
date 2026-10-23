/* Title-format resolver (header: format.h). One recursive-free pass over the
 * format with a small stack of optional sections: text appended inside a `[`
 * is kept only when a field in that section was non-empty, so an unfilled
 * section is cut back to where it started. */
#include "format.h"

#include <string.h>

#define SEL_FMT_NEST_MAX 4u

static const char* const k_field_names[SEL_FMT_FIELD_COUNT] = {"title",    "author", "format",   "subsong",
                                                               "subsongs", "duration", "elapsed", "path",
                                                               "file",     "channels", "state",   "votes"};

/* Index of the field called name[0..n), or -1. */
static int field_index(const char* name, size_t n) {
    int i;
    for (i = 0; i < SEL_FMT_FIELD_COUNT; i++) {
        if (strlen(k_field_names[i]) == n && memcmp(k_field_names[i], name, n) == 0) {
            return i;
        }
    }
    return -1;
}

/* Shared walk. v == NULL: validate only. Returns SEL_FORMAT_OK or BAD. */
static int walk(const char* fmt, const sel_format_values* v, char* out, size_t cap) {
    size_t len = 0;
    size_t start[SEL_FMT_NEST_MAX];
    int filled[SEL_FMT_NEST_MAX];
    unsigned depth = 0;
    const char* p = fmt;
    while (*p) {
        if (*p == '[') {
            if (depth >= SEL_FMT_NEST_MAX) {
                return SEL_FORMAT_BAD;
            }
            start[depth] = len;
            filled[depth] = 0;
            depth++;
            p++;
        } else if (*p == ']') {
            if (depth == 0) {
                return SEL_FORMAT_BAD;
            }
            depth--;
            if (!filled[depth]) {
                len = start[depth];
            } else if (depth > 0) {
                filled[depth - 1u] = 1;
            }
            p++;
        } else if (*p == '%') {
            const char* end = strchr(p + 1, '%');
            if (end == NULL) {
                return SEL_FORMAT_BAD;
            }
            if (end == p + 1) { /* %% */
                if (v && len + 1u < cap) {
                    out[len] = '%';
                }
                len++;
            } else {
                int f = field_index(p + 1, (size_t)(end - p - 1));
                const char* val;
                size_t n;
                if (f < 0) {
                    return SEL_FORMAT_BAD;
                }
                val = v && v->field[f] ? v->field[f] : "";
                n = strlen(val);
                if (n > 0 && depth > 0) {
                    filled[depth - 1u] = 1;
                }
                if (v) {
                    size_t room = len + 1u < cap ? cap - 1u - len : 0;
                    memcpy(out + len, val, n < room ? n : room);
                }
                len += n;
            }
            p = end + 1;
        } else {
            if (v && len + 1u < cap) {
                out[len] = *p;
            }
            len++;
            p++;
        }
    }
    if (depth != 0) {
        return SEL_FORMAT_BAD;
    }
    if (v && cap) {
        out[len < cap - 1u ? len : cap - 1u] = '\0';
    }
    return SEL_FORMAT_OK;
}

int sel_format_valid(const char* fmt) {
    return walk(fmt, NULL, NULL, 0) == SEL_FORMAT_OK;
}

int sel_format_expand(const char* fmt, const sel_format_values* v, const char* fallback, char* out, size_t cap) {
    if (cap == 0) {
        return SEL_FORMAT_OK;
    }
    if (walk(fmt, v, out, cap) == SEL_FORMAT_OK) {
        return SEL_FORMAT_OK;
    }
    if (walk(fallback, v, out, cap) != SEL_FORMAT_OK) {
        out[0] = '\0';
    }
    return SEL_FORMAT_BAD;
}
