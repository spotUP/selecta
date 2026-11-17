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

uint32_t sel_uim_scope_index(const char* name) {
    uint32_t k;
    for (k = 0; name != NULL && k < SEL_UIM_QUEUE_SCOPES; k++) {
        if (strcmp(name, k_scope_name[k]) == 0) {
            return k;
        }
    }
    return 0u;
}

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

/* I678k: the one-letter kind mark of the bitmap tier's sidebar rows (design section 12): P, A, R, C, S; a pinned row is "*". */
const char* sel_uim_library_item_mark(uint32_t kind, int pinned) {
    if (pinned) {
        return "*";
    }
    switch (kind) {
    case SEL_LIB_ITEM_ALBUM: return "A";
    case SEL_LIB_ITEM_ARTIST: return "R";
    case SEL_LIB_ITEM_COMPOSER: return "C";
    case SEL_LIB_ITEM_SOURCE: return "S";
    default: return "P";
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

/* I793: the Your Library sidebar texts and chips (declared in browse.h). */
static const char* const g_side_chip_name[SEL_UIM_SIDE_CHIPS] = {"Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"};
static const uint32_t g_side_chip_mask[SEL_UIM_SIDE_CHIPS] = {
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM),  SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ARTIST),
    SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_SOURCE), SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_LOCAL)};
static const char* const g_side_text[SEL_UIM_SIDE_TEXTS] = {
    "Your Library", "Create Playlist", "Expand Library", "Collapse Library", "All", "By You", "Nothing here yet.", "No playlists yet.",
    "Create your first playlist", "Name it, then add songs from your library", "Create playlist"};

const char* sel_uim_side_col_name(uint32_t i) {
    static const char* const k_cols[4] = {"Title", "Type", "Date Added", "Played"};
    return i < 4u ? k_cols[i] : NULL;
}

const char* sel_uim_side_chip_name(uint32_t i) {
    return i < SEL_UIM_SIDE_CHIPS ? g_side_chip_name[i] : (const char*)0;
}

uint32_t sel_uim_side_chip_mask(uint32_t i) {
    return i < SEL_UIM_SIDE_CHIPS ? g_side_chip_mask[i] : 0u;
}

const char* sel_uim_side_text_get(sel_uim_side_text id) {
    return (unsigned)id < (unsigned)SEL_UIM_SIDE_TEXTS ? g_side_text[id] : (const char*)0;
}

const char* sel_uim_side_empty(uint32_t chip, uint32_t n, int bitmap) {
    if (n != 0u) {
        return (const char*)0;
    }
    if (chip > 1u) {
        return g_side_text[SEL_UIM_SIDE_EMPTY_KIND];
    }
    return bitmap ? g_side_text[SEL_UIM_SIDE_EMPTY_NONE] : (const char*)0;
}

int sel_uim_side_card_shown(uint32_t chip, int filtered, uint32_t playlist_rows, int bitmap) {
    return chip <= 1u && !filtered && playlist_rows == 0u && !bitmap;
}

/* I1440p: the C64 SID tiles and drill kinds, shared by the desktop and text UIs (design 2026-11-16-i1440-deepsid.md 3.3, 3.4). */
static const char* const k_ds_name[SEL_UIM_DS_TILES] = {"Tags", "Charts", "Collections", "Compos", "Groups", "Players"};
static const char* const k_ds_kind[SEL_UIM_DS_TILES] = {"tags", "charts", "collections", "compos", "groups", "players"};

int sel_uim_ds_tile(uint32_t i, char* label, size_t cap, char* arg, size_t arg_cap) {
    if (i >= SEL_UIM_DS_TILES) {
        return 0;
    }
    snprintf(label, cap, "C64 SID %s", k_ds_name[i]);
    snprintf(arg, arg_cap, "deepsid\x1f%s", k_ds_kind[i]);
    return 1;
}

int sel_uim_ds_child(const char* first, const char* name, char* out, size_t cap) {
    static const char* const k[7][2] = {{"tags", "tag"},     {"charts", "chart"},   {"compos", "year"},       {"year", "event"},
                                        {"groups", "group"}, {"players", "player"}, {"collections", "folder"}};
    uint32_t i;
    for (i = 0; i < 7u; i++) {
        if (strcmp(first, k[i][0]) == 0 || (i == 6u && strcmp(first, "folder") == 0)) {
            snprintf(out, cap, "%s\x1f%s", k[i][1], name);
            return 1;
        }
    }
    return 0;
}

const char* sel_uim_ds_label(const char* first, const char* name) {
    const char* slash = strrchr(name, '/');
    return slash != NULL && (strcmp(first, "collections") == 0 || strcmp(first, "folder") == 0) ? slash + 1 : name;
}

/* I790: split the drill path at the unit separators; returns the part count. */
static uint32_t drill_split(const char* path, char (*part)[96]) {
    uint32_t n = 0u;
    const char* p = path;
    while (*p != '\0' && n < 3u) {
        size_t len = strcspn(p, "\x1f");
        snprintf(part[n], 96, "%.*s", (int)(len < 95u ? len : 95u), p);
        n++;
        p += len;
        if (*p == '\x1f') p++;
    }
    return n;
}

/* I1440s: the rule moved from the Mac drl_child (I790); the C64 SID kinds go through sel_uim_ds_child. SEL_DRILL_PAGE_ROOT is 1. */
int sel_uim_drill_child(const char* drill_path, uint32_t drill_page, const char* name, char* out, size_t cap) {
    char part[3][96];
    uint32_t np = drill_split(drill_path, part);
    out[0] = '\0';
    if (drill_page == 1u) {
        snprintf(out, cap, "%s", name);
    } else if (np >= 1u && sel_uim_ds_child(part[0], name, out, cap)) {
    } else if (np == 1u && strcmp(part[0], "formats") == 0) {
        snprintf(out, cap, "format\x1f%s", name);
    } else if (np == 1u && strcmp(part[0], "artists") == 0) {
        snprintf(out, cap, "artist\x1f%s", name);
    } else if (np == 1u && strcmp(part[0], "platforms") == 0) {
        snprintf(out, cap, "platform\x1f%s", name);
    } else if (np == 2u && strcmp(part[0], "platform") == 0) {
        snprintf(out, cap, "format\x1f%s", name);
    } else if (np == 2u && strcmp(part[0], "format") == 0) {
        snprintf(out, cap, "format\x1f%s\x1f%s", part[1], name);
    }
    return out[0] != '\0';
}

void sel_uim_drill_row_label(const char* drill_path, const char* name, char* out, size_t cap) {
    char part[3][96];
    snprintf(out, cap, "%s", drill_split(drill_path, part) > 0u ? sel_uim_ds_label(part[0], name) : name);
}
