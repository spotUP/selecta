/* The sidebar door table (MUI-P4-0): Spotify's Home, Search and Your Library, then Playlists, Queue, Now Playing and Song
 * Information. Read by the desktop UI (plugins/ui/desktop) and the MUI sidebar (plugins/ui/mui). */
#include <stddef.h>
#include <selecta/uimodel/doors.h>

static const sel_uim_door g_doors[SEL_UIM_DOOR_COUNT] = {
    {"Home", "home", NULL},   {"Search", "search", NULL},         {"Your Library", "library", NULL},
    {"Playlists", "playlists", NULL}, {"Queue", "queue", NULL}, {"Now Playing", "nowplaying", NULL},
    {"Song Information", "songinfo", "Song Info"}};

const sel_uim_door* sel_uim_door_at(uint32_t i) {
    return i < SEL_UIM_DOOR_COUNT ? &g_doors[i] : (const sel_uim_door*)0;
}
