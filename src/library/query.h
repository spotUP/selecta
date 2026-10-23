/* Library query language: parser and SQL compiler (plan 2026-10-25-m3-library.md Phase 2, I147
 * and I148; design doc section 9.2). The only place query text becomes SQL: the view model, the
 * search box, PLAY_QUERY and smart playlists all call sel_query_compile. User text only ever
 * reaches SQLite as bound parameters. No allocation: the result is a fixed struct. */
#ifndef SELECTA_LIBRARY_QUERY_H
#define SELECTA_LIBRARY_QUERY_H

#define SEL_QUERY_SQL_MAX 3072
#define SEL_QUERY_PARAMS_MAX 32
#define SEL_QUERY_TEXT_MAX 96

typedef struct sel_query_param {
    int is_int;
    long long i;
    char s[SEL_QUERY_TEXT_MAX];
} sel_query_param;

typedef struct sel_query {
    char sql[SEL_QUERY_SQL_MAX]; /* "SELECT songs.key FROM songs WHERE ..." with `?` placeholders */
    unsigned nparams;
    sel_query_param params[SEL_QUERY_PARAMS_MAX];
    unsigned terms; /* terms the parser consumed (sentinel that the parser ran) */
    char error[24]; /* "" on success, else unknown_key, view_cycle, unknown_view, syntax, bad_value, too_complex */
} sel_query;

/* Resolves a saved query by name (views table); NULL when unknown. */
typedef const char* (*sel_query_view_fn)(void* ctx, const char* name);

/* Compiles `text`. `now` (seconds since the epoch) anchors relative times. `views` may be NULL
 * (then `view:` is an unknown_view error). Returns 1 on success, 0 with out->error set. */
int sel_query_compile(const char* text, long long now, sel_query_view_fn views, void* ctx, sel_query* out);

#endif
