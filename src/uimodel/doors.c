/* The sidebar door table (MUI-P4-0): Spotify's Home, Search and Browse Folders (the Library page retired, I678i), then Playlists, Queue, Now Playing and Song
 * Information. Read by the desktop UI (plugins/ui/desktop) and the MUI sidebar (plugins/ui/mui). */
#include <stddef.h>
#include <selecta/uimodel/doors.h>

static const sel_uim_door g_doors[SEL_UIM_DOOR_COUNT] = {
    {"Home", "home", NULL, "VIEW_SOURCE home", 1u},
    {"Search", "search", NULL, "VIEW_SOURCE search", 1u},
    {"Browse Folders", "folders", "Folders", "VIEW_SHOW browse", 0u},
    {"Playlists", "playlists", NULL, "VIEW_SOURCE folders", 0u},
    {"Queue", "queue", NULL, "VIEW_SHOW queue", 0u},
    {"Now Playing", "nowplaying", NULL, NULL, 0u},
    {"Song Information", "songinfo", "Song Info", NULL, 0u}};

const sel_uim_door* sel_uim_door_at(uint32_t i) {
    return i < SEL_UIM_DOOR_COUNT ? &g_doors[i] : (const sel_uim_door*)0;
}

int sel_uim_door_shown(uint32_t i, int top_bar) {
    return i < SEL_UIM_DOOR_COUNT && !(top_bar && g_doors[i].topbar);
}

uint32_t sel_uim_door_slot(uint32_t i, int top_bar) {
    uint32_t k, n = 0;
    for (k = 0; k < i && k < SEL_UIM_DOOR_COUNT; k++) {
        n += (uint32_t)sel_uim_door_shown(k, top_bar);
    }
    return n;
}
