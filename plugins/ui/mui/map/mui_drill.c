/* I1442b MUI: the pure row and line rules of a source drill page (no MUI, no engine handle). The labels, counts, crumbs and key
 * rules are in src/uimodel/drill.c; this file only maps list rows to groups and builds the lines the page sends. */
#include <stdio.h>
#include <string.h>
#include <selecta/engine.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/drill.h>

#include "mui_map.h"

/* The i-th "name\0count\0size\0" triple of a group page, or NULL past the end. */
static const char* mui_drill_triple(const sel_collection_page* pg, uint32_t i) {
    const char* r = pg->rows;
    uint32_t k;
    if (r == NULL || i >= pg->songs) {
        return NULL;
    }
    for (k = 0u; k < i; k++) {
        r += strlen(r) + 1u;
        r += strlen(r) + 1u;
        r += strlen(r) + 1u;
    }
    return r;
}

uint32_t mui_drill_filter(const sel_collection_page* pg, const char* find, uint32_t* ord, uint32_t cap) {
    uint32_t i, n = 0u;
    int local = !sel_uim_drill_engine_find(pg->drill_path) && find[0] != '\0'; /* an engine-find level arrives narrowed */
    for (i = 0u; i < pg->songs && n < cap; i++) {
        const char* r = mui_drill_triple(pg, i);
        char lab[200];
        if (r == NULL) {
            break;
        }
        sel_uim_drill_label(pg->drill, pg->drill_path, r, lab, sizeof lab);
        if (!local || sel_uim_lfilter_match(lab, find) != 0) {
            ord[n++] = i;
        }
    }
    return n;
}

uint32_t mui_drill_rows(const sel_collection_page* pg, uint32_t skip, uint32_t vn) {
    return vn + (skip > 0u ? 1u : 0u) + (pg->songs >= (uint32_t)SEL_DRILL_WINDOW ? 1u : 0u);
}

long mui_drill_pick(const sel_collection_page* pg, uint32_t skip, const uint32_t* ord, uint32_t vn, long row) {
    long first = skip > 0u ? 1 : 0;
    if (row < 0) {
        return MUI_DRILL_NONE;
    }
    if (first == 1 && row == 0) {
        return MUI_DRILL_PREV;
    }
    if (row - first < (long)vn) {
        return (long)ord[row - first];
    }
    return row - first == (long)vn && pg->songs >= (uint32_t)SEL_DRILL_WINDOW ? MUI_DRILL_NEXT : MUI_DRILL_NONE;
}

void mui_drill_text(const sel_collection_page* pg, uint32_t skip, const uint32_t* ord, uint32_t vn, long row, char* out, size_t cap) {
    long g = mui_drill_pick(pg, skip, ord, vn, row);
    const char* r;
    char lab[200], cnt[48];
    out[0] = '\0';
    if (g == MUI_DRILL_PREV) {
        snprintf(out, cap, "Previous");
    } else if (g == MUI_DRILL_NEXT) {
        snprintf(out, cap, "Next");
    } else if (g >= 0 && (r = mui_drill_triple(pg, (uint32_t)g)) != NULL) {
        sel_uim_drill_label(pg->drill, pg->drill_path, r, lab, sizeof lab);
        r += strlen(r) + 1u;
        sel_uim_drill_count(pg->drill, strcmp(r, "0") != 0 ? r : "", cnt, sizeof cnt);
        snprintf(out, cap, "%s%s%s", lab, cnt[0] != '\0' ? "  " : "", cnt);
    }
}

uint32_t mui_drill_find_line(const char* source, const char* path, const char* find, char* out, size_t cap) {
    char key[192];
    sel_uim_drill_key(source, path, 0u, find, key, sizeof key);
    size_t n = mui_path_line("COLLECTION_OPEN 7", key, out, cap); /* the shared quoting */
    if (n == 0u || n + 3u > cap) {
        return 0u;
    }
    memcpy(out + n, " 1", 3u); /* restore flag: no Recents entry */
    return (uint32_t)(n + 2u);
}
