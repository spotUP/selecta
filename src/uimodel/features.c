/* Shared view lines and texts for the desktop and MUI front ends (MUI-P4-3..P4-16): plain C99, no allocation. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* verb + space + the text quoted with backslash and quote escaped; 0 when it does not fit. */
static size_t search_quoted_line(const char* verb, const char* text, char* out, size_t cap) {
    size_t n = 0;
    const char* p = text != NULL ? text : "";
    if (cap < 24u) {
        return 0;
    }
    n += (size_t)snprintf(out, cap, "%s \"", verb);
    for (; *p != '\0' && n + 4u < cap; p++) {
        if (*p == '"' || *p == '\\') {
            out[n++] = '\\';
        }
        out[n++] = *p;
    }
    out[n++] = '"';
    out[n] = '\0';
    return n;
}

size_t sel_uim_search_line(const char* text, char* out, size_t cap) {
    return search_quoted_line("LIBRARY_SEARCH", text, out, cap);
}

size_t sel_uim_search_all_line(const char* text, char* out, size_t cap) {
    return search_quoted_line("SOURCE_SEARCH *", text, out, cap);
}

uint32_t sel_uim_search_group(const char* heading) {
    if (heading == NULL) {
        return 0u;
    }
    if (strncmp(heading, "Songs", 5) == 0) {
        return 1u;
    }
    if (strncmp(heading, "Albums", 6) == 0) {
        return 2u;
    }
    if (strncmp(heading, "Artists", 7) == 0) {
        return 3u;
    }
    return strncmp(heading, "From ", 5) == 0 ? 4u : 0u;
}

/* I1456: chip 0 (All) clears the set, chip k flips bit k-1. */
uint32_t sel_uim_chip_toggle(uint32_t mask, uint32_t chip) {
    return chip == 0u ? 0u : chip > 32u ? mask : mask ^ (1u << (chip - 1u));
}

int sel_uim_search_group_shown(uint32_t facets, int scope_filtered, uint32_t group, int source_named) {
    if (scope_filtered) { /* I1224: a source chip over shown All results keeps only that source's From section */
        return group == 4u && source_named && sel_uim_search_group_shown(facets, 0, 4u, 0);
    }
    return facets == 0u || (group >= 1u && (facets >> ((group == 4u ? 1u : group) - 1u) & 1u) != 0u); /* union of the facets on; From sections are song results */
}

uint32_t sel_uim_search_from_n(const sel_view_row* rows, uint32_t row_n) {
    uint32_t i, n = 0;
    for (i = 0; rows != NULL && i < row_n; i++) {
        n += rows[i].kind == SEL_ROW_HEADING && sel_uim_search_group(rows[i].text) == 4u;
    }
    return n;
}

void sel_uim_search_from_name(const sel_view_row* rows, uint32_t row_n, uint32_t k, char* out, size_t cap) {
    uint32_t i;
    const char* t = "";
    const char* par;
    size_t n;
    for (i = 0; rows != NULL && i < row_n; i++) {
        if (rows[i].kind == SEL_ROW_HEADING && sel_uim_search_group(rows[i].text) == 4u && k-- == 0u) {
            t = rows[i].text + 5;
            break;
        }
    }
    par = strrchr(t, '(');
    n = par != NULL && par > t ? (size_t)(par - t) : strlen(t);
    while (n > 0 && t[n - 1] == ' ') {
        n--;
    }
    snprintf(out, cap, "%.*s", (int)n, t);
}

int sel_uim_search_row_hidden(const sel_view_row* rows, uint32_t row_n, uint32_t i, uint32_t facets, uint32_t src) {
    uint32_t h = i, from = 0, k;
    if (rows == NULL || i >= row_n) {
        return 0;
    }
    while (h > 0u && rows[h].kind != SEL_ROW_HEADING) {
        h--;
    }
    if (rows[h].kind != SEL_ROW_HEADING) {
        return 0; /* rows before any heading */
    }
    for (k = 0; k < h; k++) {
        from += rows[k].kind == SEL_ROW_HEADING && sel_uim_search_group(rows[k].text) == 4u;
    }
    return !sel_uim_search_group_shown(facets, src != 0u, sel_uim_search_group(rows[h].text), from < 32u && (src >> from & 1u) != 0u);
}

int sel_uim_slide_step(sel_uim_slide* s, const char* key, uint32_t n, int playing, uint32_t now_ms, uint32_t period_ms) {
    int advanced = 0;
    if (strcmp(s->key, key) != 0) {
        snprintf(s->key, sizeof s->key, "%s", key);
        s->idx = s->acc_ms = 0u;
        s->last_ms = now_ms;
    }
    if (now_ms != s->last_ms) {
        uint32_t dt = now_ms - s->last_ms;
        s->last_ms = now_ms;
        if (playing) s->acc_ms += dt > 5000u ? 5000u : dt;
        if (n > 1u && s->acc_ms >= period_ms) {
            s->acc_ms = 0u;
            s->idx = (s->idx + 1u) % n;
            s->switches++;
            advanced = 1;
        }
    }
    if (s->idx >= n) s->idx = 0u;
    return advanced;
}

uint32_t sel_uim_slide_period_ms(const char* pref_value) {
    uint32_t ms = (uint32_t)atoi(pref_value != NULL ? pref_value : "6") * 1000u;
    return ms < 1000u ? 6000u : ms;
}

uint32_t sel_uim_art_frames(uint32_t kinds) {
    uint32_t k, n = 0u;
    for (k = 0; k < 8u; k++) {
        n += (kinds >> k) & 1u;
    }
    return n > 0u ? n : 1u;
}

uint32_t sel_uim_art_frame_kind(uint32_t kinds, uint32_t n) {
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        if (((kinds >> k) & 1u) != 0u && n-- == 0u) return k;
    }
    return 0u;
}

int sel_uim_search_section_hidden(const sel_view_row* rows, uint32_t row_n, uint32_t heading) {
    const char* t;
    const char* par;
    const sel_view_row* next;
    if (rows == NULL || heading >= row_n || rows[heading].kind != SEL_ROW_HEADING || sel_uim_search_group(rows[heading].text) == 0u) {
        return 0;
    }
    t = rows[heading].text;
    par = strrchr(t, '(');
    if (par == NULL || strtoul(par + 1, NULL, 10) != 0ul) {
        return 0; /* a section with hits shows */
    }
    next = heading + 1u < row_n ? &rows[heading + 1u] : NULL;
    if (next == NULL || next->kind == SEL_ROW_HEADING) {
        return 1; /* 0 hits, no row under it */
    }
    /* 0 hits and one keyless state row: Searching (pending, shows once hits arrive), No results (done) and Not synced yet (an empty
     * catalogue has no hits; the Sources page still offers its sync) hide. Could not reach is a failure, not a zero-hit result, so it stays. */
    return next->key == NULL && next->text != NULL &&
           (strcmp(next->text, "Searching") == 0 || strcmp(next->text, "No results") == 0 || strcmp(next->text, "Not synced yet") == 0);
}

uint32_t sel_uim_search_section_rows(const sel_view_row* rows, uint32_t row_n, uint32_t heading) {
    uint32_t k = heading + 1u;
    if (rows == NULL || heading >= row_n) {
        return 0u;
    }
    while (k < row_n && rows[k].kind != SEL_ROW_HEADING) {
        k++;
    }
    return k - heading - 1u;
}

int sel_uim_search_row_folded(const sel_view_row* rows, uint32_t row_n, uint32_t i) {
    uint32_t h = i;
    if (rows == NULL || i >= row_n || rows[i].kind == SEL_ROW_HEADING) {
        return 0;
    }
    while (h > 0u && rows[h].kind != SEL_ROW_HEADING) {
        h--;
    }
    if (rows[h].kind != SEL_ROW_HEADING) {
        return 0; /* rows before any heading */
    }
    return i - h > SEL_UIM_SEARCH_FOLD_ROWS && sel_uim_search_section_rows(rows, row_n, h) > SEL_UIM_SEARCH_FOLD_ROWS &&
           !sel_uim_search_expanded(rows[h].text);
}

/* The expanded categories of this session: the heading text before its " (n)" count. Full table: further categories stay folded. */
enum { FOLD_KEYS = 16, FOLD_KEY_LEN = 72 };
static char fold_open[FOLD_KEYS][FOLD_KEY_LEN];
static uint32_t fold_open_n;

/* The key of a heading: its text before the " (n)" count. Returns its length. */
static size_t fold_key(const char* heading, char* key) {
    const char* par = heading != NULL ? strrchr(heading, '(') : NULL;
    size_t n = heading == NULL ? 0u : par != NULL ? (size_t)(par - heading) : strlen(heading);
    while (n > 0u && heading[n - 1u] == ' ') {
        n--;
    }
    n = n < FOLD_KEY_LEN - 1u ? n : FOLD_KEY_LEN - 1u;
    if (n > 0u) {
        memcpy(key, heading, n);
    }
    key[n] = '\0';
    return n;
}

static int fold_find(const char* key) {
    uint32_t i;
    for (i = 0; i < fold_open_n; i++) {
        if (strcmp(fold_open[i], key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int sel_uim_search_expanded(const char* heading) {
    char key[FOLD_KEY_LEN];
    return fold_key(heading, key) > 0u && fold_find(key) >= 0;
}

void sel_uim_search_fold_toggle(const char* heading) {
    char key[FOLD_KEY_LEN];
    int i;
    if (fold_key(heading, key) == 0u) {
        return;
    }
    i = fold_find(key);
    if (i >= 0) {
        memcpy(fold_open[i], fold_open[fold_open_n - 1u], FOLD_KEY_LEN);
        fold_open_n--;
    } else if (fold_open_n < FOLD_KEYS) {
        memcpy(fold_open[fold_open_n++], key, FOLD_KEY_LEN);
    }
}

const char* sel_uim_song_menu_label(uint32_t item, int on_queue) {
    static const char* const items[SEL_UIM_SONG_MENU_ITEMS] = {"Play", "Play Next", "Add to Queue", "Add to Playlist", "Remove from Library"};
    if (item >= SEL_UIM_SONG_MENU_ITEMS) {
        return NULL;
    }
    return item == 4u && on_queue ? "Remove from Queue" : items[item];
}

void sel_uim_rate_line(uint32_t current, uint32_t star, char* out, size_t cap) {
    snprintf(out, cap, "RATE %u", (unsigned)(star > 5u ? 0u : current == star ? 0u : star));
}

void sel_uim_remove_text(const char* title, char* out, size_t cap) {
    snprintf(out, cap, "Remove \"%s\" from the library? The audio file on disk is not touched.", title != NULL ? title : "");
}

void sel_uim_clear_text(uint32_t songs, uint32_t playlists, char* out, size_t cap) {
    snprintf(out, cap, "Remove %u songs and %u playlists, with all ratings and notes? This cannot be undone. Files on disk are not touched.",
             (unsigned)songs, (unsigned)playlists);
}

typedef struct sel_uim_dlg_row { const char* title; const char* confirm; const char* mui_confirm; const char* text; } sel_uim_dlg_row;
static const sel_uim_dlg_row g_dlg[SEL_UIM_DLG_COUNT] = {
    [SEL_UIM_DLG_REMOVE] = {"Remove from Library", "Remove", "Remove", NULL},
    [SEL_UIM_DLG_CLEAR] = {"Clear Library", "Clear Library", "Clear", NULL},
    [SEL_UIM_DLG_QUIT] = {"Quit", "Quit", "Quit", "Quit Selecta?"},
};

const char* sel_uim_dlg_title(sel_uim_dlg_kind k) { return (unsigned)k < (unsigned)SEL_UIM_DLG_COUNT ? g_dlg[k].title : NULL; }
const char* sel_uim_dlg_confirm_label(sel_uim_dlg_kind k) { return (unsigned)k < (unsigned)SEL_UIM_DLG_COUNT ? g_dlg[k].confirm : NULL; }
const char* sel_uim_dlg_mui_confirm_label(sel_uim_dlg_kind k) { return (unsigned)k < (unsigned)SEL_UIM_DLG_COUNT ? g_dlg[k].mui_confirm : NULL; }
const char* sel_uim_dlg_text(sel_uim_dlg_kind k) { return (unsigned)k < (unsigned)SEL_UIM_DLG_COUNT ? g_dlg[k].text : NULL; }
const char* sel_uim_dlg_cancel_label(void) { return "Cancel"; }

void sel_uim_dlg_hint(sel_uim_dlg_kind k, char* out, size_t cap) {
    snprintf(out, cap, "Enter %s  Esc %s", sel_uim_dlg_confirm_label(k), sel_uim_dlg_cancel_label());
}

void sel_uim_dlg_mui_gadgets(sel_uim_dlg_kind k, char* out, size_t cap) {
    snprintf(out, cap, "*%s|%s", sel_uim_dlg_mui_confirm_label(k), sel_uim_dlg_cancel_label());
}

const char* sel_uim_state_text(uint32_t view, uint32_t kind) {
    static const char* const empty[SEL_UIM_SV_COUNT] = {
        "The library is empty. Add a folder to scan.", "The queue is empty. Add songs from the Library.",
        "No results. Try a different search.", "No playlists yet. Type a name and choose New Playlist.",
        "Nothing is playing. Play a song to see its details.", "No lyrics for this song.", "No liked songs yet. Like a song to see it here."};
    static const char* const loading[SEL_UIM_SV_COUNT] = {
        "Loading the library...", "Loading the queue...", "Searching...", "Loading playlists...",
        "Loading the song...", "Loading lyrics...", "Loading liked songs..."};
    static const char* const error[SEL_UIM_SV_COUNT] = {
        "The library could not be read.", "The queue could not be read.", "The search failed.", "The playlists could not be read.",
        "The song could not be loaded.", "The lyrics could not be loaded.", "Liked songs could not be read."};
    if (view >= SEL_UIM_SV_COUNT) {
        return NULL;
    }
    return kind == SEL_UIM_ST_EMPTY ? empty[view] : kind == SEL_UIM_ST_LOADING ? loading[view] : kind == SEL_UIM_ST_ERROR ? error[view] : NULL;
}

static const sel_uim_pref_row g_pref_rows[] = {
    {"mix.loudness", "Loudness", SEL_UIM_PREF_CYCLE, "track,auto,off", SEL_UIM_MACH_ALL},
    {"dsp.crossfeed", "Crossfeed", SEL_UIM_PREF_CYCLE, "auto,on,off", SEL_UIM_MACH_ALL},
    {"output.led", "Power LED Filter", SEL_UIM_PREF_CHECK, "off,on", SEL_UIM_MACH_ALL},
    {"mix.stereo_width", "Stereo Width", SEL_UIM_PREF_SLIDER, "0,100", SEL_UIM_MACH_ALL},
    {"library.prefetch", "Prefetch Next Song", SEL_UIM_PREF_CHECK, "off,on", SEL_UIM_MACH_ALL},
    {"library.metered", "Metered Connection", SEL_UIM_PREF_CHECK, "off,on", SEL_UIM_MACH_ALL},
    {"ui.desktop.overscan", "Fill Screen (Overscan)", SEL_UIM_PREF_CHECK, "off,on", SEL_UIM_MACH_AMIGA},
    {"playsid.filter", "reSID Filter", SEL_UIM_PREF_CYCLE, "internal,internal+external,off", SEL_UIM_MACH_AMIGA},
    {"playsid.boost", "reSID Boost", SEL_UIM_PREF_CYCLE, "off,2x,3x,4x", SEL_UIM_MACH_AMIGA},
    {"playsid.oversample", "reSID Oversampling", SEL_UIM_PREF_CYCLE, "normal,2x,3x,4x", SEL_UIM_MACH_AMIGA},
    {"sid.model", "Chip Model", SEL_UIM_PREF_CYCLE, "auto,6581,8580", SEL_UIM_MACH_HOST},
    {"sid.clock", "Video Standard", SEL_UIM_PREF_CYCLE, "auto,pal,ntsc", SEL_UIM_MACH_HOST},
    {"sid.filter", "Chip Filter", SEL_UIM_PREF_CHECK, "off,on", SEL_UIM_MACH_HOST},
    {"sid.digiboost", "Digi Boost", SEL_UIM_PREF_CYCLE, "auto,on,off", SEL_UIM_MACH_HOST},
    {"sid.rom.kernal", "C64 KERNAL ROM", SEL_UIM_PREF_ROM, "kernal", SEL_UIM_MACH_HOST},
    {"sid.rom.basic", "C64 BASIC ROM", SEL_UIM_PREF_ROM, "basic", SEL_UIM_MACH_HOST},
    {"sid.rom.chargen", "C64 Character ROM", SEL_UIM_PREF_ROM, "chargen", SEL_UIM_MACH_HOST},
    {"view.title_format", "Title Format", SEL_UIM_PREF_STRING, "", SEL_UIM_MACH_ALL},
    {"view.status_format", "Status Format", SEL_UIM_PREF_STRING, "", SEL_UIM_MACH_ALL},
};

typedef char sel_uim_pref_rows_fit[(sizeof g_pref_rows / sizeof g_pref_rows[0]) <= SEL_UIM_PREF_ROWS_MAX ? 1 : -1];

uint32_t sel_uim_machine_here(void) {
#if defined(__amigaos__) || defined(AMIGA)
    return SEL_UIM_MACH_AMIGA;
#else
    return SEL_UIM_MACH_HOST;
#endif
}

uint32_t sel_uim_pref_row_count_for(uint32_t machine) {
    uint32_t i, n = 0;
    for (i = 0; i < (uint32_t)(sizeof g_pref_rows / sizeof g_pref_rows[0]); i++) {
        n += (g_pref_rows[i].machines & machine) != 0u;
    }
    return n;
}

const sel_uim_pref_row* sel_uim_pref_row_at_for(uint32_t machine, uint32_t i) {
    uint32_t k;
    for (k = 0; k < (uint32_t)(sizeof g_pref_rows / sizeof g_pref_rows[0]); k++) {
        if ((g_pref_rows[k].machines & machine) != 0u && i-- == 0u) {
            return &g_pref_rows[k];
        }
    }
    return NULL;
}

uint32_t sel_uim_pref_row_count(void) {
    return sel_uim_pref_row_count_for(sel_uim_machine_here());
}

const sel_uim_pref_row* sel_uim_pref_row_at(uint32_t i) {
    return sel_uim_pref_row_at_for(sel_uim_machine_here(), i);
}

uint32_t sel_uim_pref_choice(const char* choices, uint32_t n, char* out, size_t cap) {
    uint32_t i = 0, count = 1;
    const char* p = choices;
    out[0] = '\0';
    for (; *p != '\0'; p++) {
        count += *p == ',';
    }
    p = choices;
    while (i < n && strchr(p, ',') != NULL) {
        p = strchr(p, ',') + 1;
        i++;
    }
    if (i == n) {
        const char* e = strchr(p, ',');
        size_t len = e != NULL ? (size_t)(e - p) : strlen(p);
        len = len < cap - 1u ? len : cap - 1u;
        memcpy(out, p, len);
        out[len] = '\0';
    }
    return count;
}

uint32_t sel_uim_pref_choice_index(const char* choices, const char* cur) {
    char c[48];
    uint32_t n = sel_uim_pref_choice(choices, 0u, c, sizeof c), i;
    for (i = 0; i < n; i++) {
        sel_uim_pref_choice(choices, i, c, sizeof c);
        if (strcmp(c, cur) == 0) {
            return i;
        }
    }
    return 0u;
}

void sel_uim_pref_line(const char* key, const char* value, char* out, size_t cap) {
    snprintf(out, cap, "PREF_SET %s \"%s\"", key, value != NULL ? value : "");
}
