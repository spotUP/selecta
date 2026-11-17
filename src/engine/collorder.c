/* I994d1: see collorder.h. */
#include "collorder.h"

#include <ctype.h>
#include <selecta/uimodel/browse.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

int sel_coll_tag_has(const char* tags, const char* tag) {
    size_t tl;
    if (tag == NULL || tag[0] == '\0') return 1;
    if (tags == NULL) return 0;
    tl = strlen(tag);
    while (*tags != '\0') {
        const char* e = strstr(tags, ", ");
        size_t len = e != NULL ? (size_t)(e - tags) : strlen(tags);
        if (len == tl && strncasecmp(tags, tag, tl) == 0) return 1;
        if (e == NULL) break;
        tags = e + 2;
    }
    return 0;
}

void sel_coll_tags_merge(char* all, uint32_t cap, const char* tags) {
    char* end = all;
    if (all == NULL || cap < 2u || tags == NULL) return;
    while (end[0] != '\0') end += strlen(end) + 1u;
    while (*tags != '\0') {
        const char* e = strstr(tags, ", ");
        size_t len = e != NULL ? (size_t)(e - tags) : strlen(tags);
        char name[96];
        const char* p;
        int have = 0;
        if (len > 0u && len < sizeof name) {
            memcpy(name, tags, len);
            name[len] = '\0';
            for (p = all; p[0] != '\0'; p += strlen(p) + 1u) have |= strcasecmp(p, name) == 0;
            if (!have && (size_t)(end - all) + len + 2u <= cap) {
                memcpy(end, name, len + 1u);
                end += len + 1u;
                end[0] = '\0';
            }
        }
        if (e == NULL) break;
        tags = e + 2;
    }
}

uint32_t sel_coll_order_build(const sel_coll_rec* rec, uint32_t n, uint32_t sort, uint32_t descending, const char* filter, const char* tag,
                              uint32_t* out) {
    uint32_t i, shown = 0u;
    for (i = 0; i < n; i++) {
        if (!sel_coll_tag_has(rec[i].tags, tag)) continue;
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
