/* ISO-8859-1 <-> UTF-8 for the OS boundary (I218d). Strings inside the engine are UTF-8; AmigaDOS file names are
 * Latin-1 bytes. The Amiga OS layer (src/core/amiga) converts names read from DOS to UTF-8 and paths given to DOS to
 * Latin-1; the Hippo catalogue importer (legacy Latin-1 feeds) shares the first direction. */
#ifndef SELECTA_LIBRARY_LATIN1_H
#define SELECTA_LIBRARY_LATIN1_H

#include <stddef.h>

/* `len` Latin-1 bytes of `src` to UTF-8 in `dst` (capacity n), always NUL terminated; stops at the last whole
 * character that fits (truncates at n - 1). Returns the bytes written, without the NUL. n == 0 writes nothing. */
size_t sel_latin1_to_utf8(char* dst, size_t n, const char* src, size_t len);

/* NUL terminated UTF-8 `src` to Latin-1 in `dst` (capacity n), NUL terminated. Returns 1 on success. Returns 0
 * (dst holds an empty string) when `src` has a code point above U+00FF, is not valid UTF-8, or does not fit: such a
 * name cannot exist on a Latin-1 volume, and substituting '?' would name a different (or wildcard) file, so the
 * caller fails the call instead. */
int sel_utf8_to_latin1(char* dst, size_t n, const char* src);

/* 1 when every byte of `s` is below 0x80 (conversion is the identity). */
int sel_latin1_is_ascii(const char* s);

#endif /* SELECTA_LIBRARY_LATIN1_H */
