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

void sel_uim_pref_line(const char* key, const char* value, char* out, size_t cap) {
    snprintf(out, cap, "PREF_SET %s \"%s\"", key, value != NULL ? value : "");
}
