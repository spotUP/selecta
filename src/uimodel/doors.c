/* The sidebar door table (MUI-P4-0): Spotify's Home, Search and Your Library, then Playlists, Queue, Now Playing and Song
 * Information. Read by the desktop UI (plugins/ui/desktop) and the MUI sidebar (plugins/ui/mui). */
#include <stddef.h>
#include <selecta/uimodel/doors.h>

static const sel_uim_door g_doors[SEL_UIM_DOOR_COUNT] = {
    {"Home", "home", NULL, 1u},   {"Search", "search", NULL, 1u},         {"Your Library", "library", NULL, 0u},
    {"Playlists", "playlists", NULL, 0u}, {"Queue", "queue", NULL, 0u}, {"Now Playing", "nowplaying", NULL, 0u},
    {"Song Information", "songinfo", "Song Info", 0u}};

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
