/* I994d1: see collorder.h. */
#include "collorder.h"

#include <ctype.h>
#include <selecta/uimodel/browse.h>
#include <stdlib.h>
#include <string.h>

const char* sel_coll_title(const sel_coll_rec* r) {
    const char* b;
    if (r->title != NULL && r->title[0] != '\0') return r->title;
    if (r->key == NULL) return "";
    b = strrchr(r->key, '/');
    return b != NULL ? b + 1 : r->key;
}

/* qsort has no context argument; the engine builds on the one read slot at a time. */
static const sel_coll_rec* g_rec;
static uint32_t g_sort, g_desc;

static int ci_cmp(const char* a, const char* b) {
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x != y) return x < y ? -1 : 1;
    }
    return (*a != '\0') - (*b != '\0');
}

static int cmp_i64(int64_t a, int64_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

static int row_cmp(const void* pa, const void* pb) {
    uint32_t x = *(const uint32_t*)pa, y = *(const uint32_t*)pb;
    const sel_coll_rec *a = &g_rec[x], *b = &g_rec[y];
    int c = 0;
    switch (g_sort) {
    case SEL_COLL_SORT_TITLE: c = ci_cmp(sel_coll_title(a), sel_coll_title(b)); break;
    case SEL_COLL_SORT_ARTIST: c = ci_cmp(a->artist, b->artist); break;
    case SEL_COLL_SORT_ALBUM: c = ci_cmp(a->album, b->album); break;
    case SEL_COLL_SORT_ADDED: c = cmp_i64(a->added, b->added); break;
    case SEL_COLL_SORT_DURATION: c = cmp_i64(a->duration_ms, b->duration_ms); break;
    default: break;
    }
    if (c != 0) return g_desc ? -c : c;
    return x < y ? -1 : x > y ? 1 : 0; /* ties: stored index, ascending in both directions */
}

static int field_has(const char* s, const char* filter) {
    return sel_uim_lfilter_match(s != NULL ? s : "", filter);
}

uint32_t sel_coll_order_build(const sel_coll_rec* rec, uint32_t n, uint32_t sort, uint32_t descending, const char* filter, uint32_t* out) {
    uint32_t i, shown = 0u;
    for (i = 0; i < n; i++) {
        if (filter != NULL && filter[0] != '\0' && !field_has(sel_coll_title(&rec[i]), filter) && !field_has(rec[i].artist, filter) &&
            !field_has(rec[i].album, filter)) {
            continue;
        }
        out[shown++] = i;
    }
    if (sort == SEL_COLL_SORT_CUSTOM) {
        if (descending) {
            for (i = 0; i < shown / 2u; i++) {
                uint32_t t = out[i];
                out[i] = out[shown - 1u - i];
                out[shown - 1u - i] = t;
            }
        }
        return shown;
    }
    g_rec = rec;
    g_sort = sort;
    g_desc = descending;
    qsort(out, shown, sizeof out[0], row_cmp);
    return shown;
}
