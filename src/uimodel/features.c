/* Shared view lines and texts for the desktop and MUI front ends (MUI-P4-3..P4-16): plain C99, no allocation. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

size_t sel_uim_search_line(const char* text, char* out, size_t cap) {
    size_t n = 0;
    const char* p = text != NULL ? text : "";
    if (cap < 24u) {
        return 0;
    }
    n += (size_t)snprintf(out, cap, "LIBRARY_SEARCH \"");
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
    {"mix.loudness", "Loudness", SEL_UIM_PREF_CYCLE, "track,auto,off"},
    {"dsp.crossfeed", "Crossfeed", SEL_UIM_PREF_CYCLE, "auto,on,off"},
    {"output.led", "Power LED Filter", SEL_UIM_PREF_CHECK, "off,on"},
    {"mix.stereo_width", "Stereo Width", SEL_UIM_PREF_SLIDER, "0,100"},
    {"library.prefetch", "Prefetch Next Song", SEL_UIM_PREF_CHECK, "off,on"},
    {"library.metered", "Metered Connection", SEL_UIM_PREF_CHECK, "off,on"},
    {"view.title_format", "Title Format", SEL_UIM_PREF_STRING, ""},
    {"view.status_format", "Status Format", SEL_UIM_PREF_STRING, ""},
};

uint32_t sel_uim_pref_row_count(void) {
    return (uint32_t)(sizeof g_pref_rows / sizeof g_pref_rows[0]);
}

const sel_uim_pref_row* sel_uim_pref_row_at(uint32_t i) {
    return i < sel_uim_pref_row_count() ? &g_pref_rows[i] : NULL;
}

void sel_uim_pref_line(const char* key, const char* value, char* out, size_t cap) {
    snprintf(out, cap, "PREF_SET %s \"%s\"", key, value != NULL ? value : "");
}
