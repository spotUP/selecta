/* Library modes, queue scopes and tile steps: see include/selecta/uimodel/browse.h. */
#include <selecta/uimodel/browse.h>
#include <selecta/library_item.h>

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


/* "\xe2\x80\xa2" is U+2022 BULLET, in the desktop atlas and the Amiga font map. */
#define BULLET " \xe2\x80\xa2 "

uint32_t sel_uim_library_item_subtitle(uint32_t kind, const char* sub, uint32_t songs, char* out, uint32_t cap) {
    char count[32];
    int n;

    snprintf(count, sizeof count, "%u %s", (unsigned)songs, songs == 1u ? "song" : "songs");
    if (sub == NULL) sub = "";
    switch (kind) {
    case SEL_LIB_ITEM_PLAYLIST:
        n = snprintf(out, cap, "%s", sub); /* the owner name ("You") */
        break;
    case SEL_LIB_ITEM_ALBUM:
        n = sub[0] != '\0' ? snprintf(out, cap, "Album" BULLET "%s", sub) : snprintf(out, cap, "Album");
        break;
    case SEL_LIB_ITEM_ARTIST: n = snprintf(out, cap, "Artist"); break;
    case SEL_LIB_ITEM_COMPOSER: n = snprintf(out, cap, "Composer" BULLET "%s", count); break;
    case SEL_LIB_ITEM_SOURCE: n = snprintf(out, cap, "Source" BULLET "%s", count); break;
    default: /* the pinned kinds Liked, Recent, Local are playlists of the user's songs */
        n = snprintf(out, cap, "Playlist" BULLET "%s", count);
        break;
    }
    return n > 0 ? (uint32_t)n : 0u;
}

/* I678d: the Type column and the compact row of Your Library (design section 6): the kind as one word. */
const char* sel_uim_library_item_type(uint32_t kind) {
    switch (kind) {
    case SEL_LIB_ITEM_ALBUM: return "Album";
    case SEL_LIB_ITEM_ARTIST: return "Artist";
    case SEL_LIB_ITEM_COMPOSER: return "Composer";
    case SEL_LIB_ITEM_SOURCE: return "Source";
    default: return "Playlist"; /* the playlists and the pinned kinds, which are playlists of the user's songs */
    }
}

/* I678d: "2 days ago" for the Date Added and Played columns. `then` 0 (never) or a value that is not an epoch time (a playlist's
 * creation order) writes "-"; a time in the future counts as now. One formatter for every UI. */
uint32_t sel_uim_relative_time(int64_t now, uint32_t then, char* out, uint32_t cap) {
    int64_t s = now - (int64_t)then, n;
    const char* unit;
    int w;
    if (then < 1000000000u) {
        w = snprintf(out, cap, "-");
    } else if (s < 60) {
        w = snprintf(out, cap, "Just now");
    } else {
        if (s < 3600) {
            n = s / 60, unit = "minute";
        } else if (s < 86400) {
            n = s / 3600, unit = "hour";
        } else if (s < 30 * 86400) {
            n = s / 86400, unit = "day";
        } else if (s < 365 * 86400) {
            n = s / (30 * 86400), unit = "month";
        } else {
            n = s / (365 * 86400), unit = "year";
        }
        w = snprintf(out, cap, "%lld %s%s ago", (long long)n, unit, n == 1 ? "" : "s");
    }
    return w > 0 ? (uint32_t)w : 0u;
}
