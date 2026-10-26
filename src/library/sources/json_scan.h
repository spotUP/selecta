/* A tiny streaming JSON reader over a FILE*, shared by the sources whose list is JSON (asma_sync.c, zxart_sync.c).
 * It reads no document tree: callers walk the keys they want and skip the rest. */
#ifndef SELECTA_LIBRARY_SOURCES_JSON_SCAN_H
#define SELECTA_LIBRARY_SOURCES_JSON_SCAN_H

#include <stddef.h>
#include <stdio.h>

/* Reads a string whose opening quote is consumed. Stores it (UTF-8, cut to `n` - 1 bytes) in `out` when not NULL.
 * Returns 0 on a malformed string or end of file. */
int sel_json_read_string(FILE* f, char* out, size_t n);

/* The next byte that is not JSON white space (EOF at the end). */
int sel_json_skip_ws(FILE* f);

/* Skips a value whose first byte `c` is read: a string, number or literal, or a nested object or array. Leaves the
 * delimiter that ends it (',' '}' ']') in `*end`. 0 on malformed input or end of file. */
int sel_json_skip_value(FILE* f, int c, int* end);

#endif
