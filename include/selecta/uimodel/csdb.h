/* I1440l2 (design 2026-11-16-i1440-deepsid.md 3.2): the CSDb release rows of Song Information, shared by every UI and by the library fetcher. A row's
 * value ends in "(csdb.dk/release/?id=N)" (sel_deepsid_csdb_rows); these read the release id back, build the page URL a click opens, and name the artwork
 * key under which the fetcher stores the release screenshot (the UI draws it like any cover). Pure, header-only: the library does not link the uimodel. */
#ifndef SELECTA_UIMODEL_CSDB_H
#define SELECTA_UIMODEL_CSDB_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The release id of a "CSDb release" row value, 0 when the value is not one. */
static inline int sel_csdb_row_id(const char* value) {
    const char* p = value != NULL ? strstr(value, "csdb.dk/release/?id=") : NULL;
    return p != NULL ? atoi(p + 20) : 0;
}

static inline void sel_csdb_url(int id, char* out, size_t cap) {
    snprintf(out, cap, "https://csdb.dk/release/?id=%d", id);
}

static inline void sel_csdb_art_key(int id, char* out, size_t cap) {
    snprintf(out, cap, "csdb-release:%d", id);
}

#endif
