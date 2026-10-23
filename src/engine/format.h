/* Title formats (plan M1.2 phase 1, row I67): a foobar2000 subset.
 *   %field%   the value of a field (unknown field: bad format)
 *   [ ... ]   optional section, dropped when every field in it is empty
 *   %%        a literal percent sign
 * No functions ($if and friends) in M1. The view model calls this, so UIs get
 * finished strings. No float, no allocation. */
#ifndef SELECTA_ENGINE_FORMAT_H
#define SELECTA_ENGINE_FORMAT_H

#include <stddef.h>

typedef enum sel_format_field {
    SEL_FMT_TITLE = 0,
    SEL_FMT_AUTHOR,
    SEL_FMT_FORMAT,
    SEL_FMT_SUBSONG,
    SEL_FMT_SUBSONGS,
    SEL_FMT_DURATION,
    SEL_FMT_ELAPSED,
    SEL_FMT_PATH,
    SEL_FMT_FILE,
    SEL_FMT_CHANNELS,
    SEL_FMT_STATE,
    SEL_FMT_VOTES, /* HippoPlayer vote count of the song, empty until a VOTE answer is stored (I174a) */
    SEL_FMT_FIELD_COUNT
} sel_format_field;

/* Field values; NULL and "" both mean empty. Strings are borrowed. */
typedef struct sel_format_values {
    const char* field[SEL_FMT_FIELD_COUNT];
} sel_format_values;

enum { SEL_FORMAT_OK = 0, SEL_FORMAT_BAD = 1 }; /* BAD: warning bad_format */

/* 1 when `fmt` parses (known fields, balanced brackets, no lone %). */
int sel_format_valid(const char* fmt);

/* Expands `fmt` into `out` (always NUL-terminated, truncated to `cap`). A bad
 * `fmt` expands `fallback` instead and returns SEL_FORMAT_BAD. */
int sel_format_expand(const char* fmt, const sel_format_values* v, const char* fallback, char* out, size_t cap);

#endif /* SELECTA_ENGINE_FORMAT_H */
