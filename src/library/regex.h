/* Regular-expression subset for the library query language (see regex.c). */
#ifndef SELECTA_LIBRARY_REGEX_H
#define SELECTA_LIBRARY_REGEX_H

/* 1: `pattern` matches somewhere in `text` (case-insensitive), 0: no match, -1: malformed pattern. */
int sel_regex_match(const char* pattern, const char* text);

#endif
