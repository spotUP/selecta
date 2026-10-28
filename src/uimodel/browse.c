/* Library modes, queue scopes and tile steps: see include/selecta/uimodel/browse.h. */
#include <selecta/uimodel/browse.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char* const k_mode_name[SEL_UIM_LIB_MODES] = {"Composers", "Tree", "Columns", "Files", "History", "Sources"};
static const char* const k_scope_name[SEL_UIM_QUEUE_SCOPES] = {"library", "album", "playlist"};
static const char* const k_scope_label[SEL_UIM_QUEUE_SCOPES] = {"Scope: Library", "Scope: Album", "Scope: Playlist"};
static const int32_t k_tile_steps[SEL_UIM_TILE_STEPS] = {96, 120, 144, 176, 208};

static uint32_t put(char out[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX], uint32_t n, const char* text) {
    snprintf(out[n], SEL_UIM_LIB_LINE_MAX, "%s", text);
    return n + 1u;
}

const char* sel_uim_libmode_name(uint32_t mode) { return mode < SEL_UIM_LIB_MODES ? k_mode_name[mode] : NULL; }

uint32_t sel_uim_libmode_lines(uint32_t mode, uint32_t window_rows, char out[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX]) {
    char win[SEL_UIM_LIB_LINE_MAX];
    uint32_t n = 0;
    snprintf(win, sizeof win, "VIEW_WINDOW %u", (unsigned)window_rows);
    if (mode == SEL_UIM_LIB_SOURCES) {
        n = put(out, n, "SOURCES");
    } else if (mode == 0u) {
        n = put(out, n, "VIEW_STYLE regular");
        n = put(out, n, "VIEW_SOURCE queue");
    } else if (mode == 3u || mode == 4u) {
        n = put(out, n, "VIEW_STYLE regular");
        n = put(out, n, mode == 3u ? "VIEW_SOURCE browse" : "VIEW_SOURCE home");
        n = put(out, n, win);
    } else if (mode < SEL_UIM_LIB_MODES) {
        n = put(out, n, "LIBRARY_SEARCH \"\"");
        n = put(out, n, "VIEW_SOURCE query");
        n = put(out, n, win);
        n = put(out, n, mode == 1u ? "VIEW_STYLE tree" : "VIEW_STYLE drilldown");
        n = put(out, n, "VIEW_GROUP format");
    }
    return n;
}

const char* sel_uim_scope_name(uint32_t i) { return i < SEL_UIM_QUEUE_SCOPES ? k_scope_name[i] : NULL; }
const char* sel_uim_scope_label(uint32_t i) { return i < SEL_UIM_QUEUE_SCOPES ? k_scope_label[i] : NULL; }

uint32_t sel_uim_scope_line(uint32_t i, char* out, uint32_t cap) {
    int n;
    if (i >= SEL_UIM_QUEUE_SCOPES || out == NULL || cap == 0u) {
        return 0;
    }
    n = snprintf(out, cap, "SCOPE %s", k_scope_name[i]);
    return n > 0 ? (uint32_t)n : 0u;
}

int32_t sel_uim_tile_step(uint32_t i) { return i < SEL_UIM_TILE_STEPS ? k_tile_steps[i] : 0; }

static const char* const k_lsort[SEL_UIM_LSORTS] = {"Recents", "Recently added", "Alphabetical", "Creator"};
static const char* const k_lview[SEL_UIM_LVIEWS] = {"Compact list", "List", "Small grid", "Large grid"};

const char* sel_uim_lsort_label(uint32_t i) { return i < SEL_UIM_LSORTS ? k_lsort[i] : NULL; }
const char* sel_uim_lview_label(uint32_t i) { return i < SEL_UIM_LVIEWS ? k_lview[i] : NULL; }

static int32_t lower_cmp(const char* a, const char* b) {
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

int32_t sel_uim_lsort_cmp(uint32_t mode, const char* a_name, uint32_t a_added, const char* a_creator, const char* b_name,
                          uint32_t b_added, const char* b_creator) {
    int32_t c;
    switch (mode) {
    case SEL_UIM_LSORT_ADDED:
        return a_added == b_added ? 0 : (a_added > b_added ? -1 : 1);
    case SEL_UIM_LSORT_ALPHA:
        return lower_cmp(a_name, b_name);
    case SEL_UIM_LSORT_CREATOR:
        c = lower_cmp(a_creator, b_creator);
        return c != 0 ? c : lower_cmp(a_name, b_name);
    default:
        return 0;
    }
}

int32_t sel_uim_lfilter_match(const char* name, const char* filter) {
    size_t i, n;
    if (filter == NULL || filter[0] == '\0') return 1;
    n = strlen(filter);
    for (i = 0; name[i] != '\0'; i++) {
        size_t k = 0;
        while (k < n && name[i + k] != '\0' && tolower((unsigned char)name[i + k]) == tolower((unsigned char)filter[k])) k++;
        if (k == n) return 1;
    }
    return 0;
}
