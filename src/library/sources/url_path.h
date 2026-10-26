/* Percent-encoding of a source path under a fixed root URL, shared by the sources whose files sit below one folder
 * (asma.c, sndh.c). */
#ifndef SELECTA_LIBRARY_SOURCES_URL_PATH_H
#define SELECTA_LIBRARY_SOURCES_URL_PATH_H

#include <stddef.h>

/* `root` + `path` with everything but letters, digits and `/-_.~` percent-encoded. 0 when `path` is empty or
 * absolute, or the result does not fit `n`. */
int sel_url_path_encode(const char* root, const char* path, char* out, size_t n);

/* The inverse: the decoded rest of `url` after `root`. 0 when `url` does not start with `root`, has a bad escape, is
 * empty after the root or does not fit `n`. */
int sel_url_path_decode(const char* root, const char* url, char* path, size_t n);

/* `s` with its %XX escapes decoded into `out`. 0 on a bad escape or when it does not fit `n`. */
int sel_url_unquote(const char* s, char* out, size_t n);

#endif
