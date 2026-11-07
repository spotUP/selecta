/* I685g: the Selecta side of the icon names (see mui_iconmap.h): the button key to sel_gui_icon table and the enum to Lucide name map. */
#include "mui_iconmap.h"

#include <stddef.h>
#include <string.h>

#include <selecta/gui/gui_icons.h>

static uint32_t g_resolved;

static const struct {
    const char* key;
    int icon;
} k_icons[] = {
    /* transport and extra actions (uimodel action ids) */
    {"prev", SEL_GUI_ICON_SKIP_BACK}, {"play", SEL_GUI_ICON_PLAY}, {"pause", SEL_GUI_ICON_PAUSE}, {"next", SEL_GUI_ICON_SKIP_FORWARD},
    {"stop", SEL_GUI_ICON_SQUARE}, {"repeat_all", SEL_GUI_ICON_REPEAT}, {"repeat_one", SEL_GUI_ICON_REPEAT}, {"repeat_off", SEL_GUI_ICON_REPEAT},
    {"random_on", SEL_GUI_ICON_SHUFFLE}, {"random_off", SEL_GUI_ICON_SHUFFLE}, {"next_album", SEL_GUI_ICON_CHEVRON_RIGHT},
    {"prev_album", SEL_GUI_ICON_CHEVRON_LEFT}, {"pitch_up", SEL_GUI_ICON_CHEVRON_UP}, {"pitch_down", SEL_GUI_ICON_CHEVRON_DOWN},
    {"tempo_up", SEL_GUI_ICON_PLUS}, {"tempo_down", SEL_GUI_ICON_MINUS},
    /* sidebar doors (page names) */
    {"nav_back", SEL_GUI_ICON_CHEVRON_LEFT}, {"nav_forward", SEL_GUI_ICON_CHEVRON_RIGHT}, /* I794: top bar Back / Forward */
    {"home", SEL_GUI_ICON_HOUSE}, {"search", SEL_GUI_ICON_SEARCH}, {"library", SEL_GUI_ICON_LIBRARY}, {"folders", SEL_GUI_ICON_LIBRARY}, {"playlists", SEL_GUI_ICON_MUSIC},
    {"queue", SEL_GUI_ICON_LIST}, {"nowplaying", SEL_GUI_ICON_DISC}, {"songinfo", SEL_GUI_ICON_INFO},
    /* toolbar (mui_tool_labels) */
    {"Like", SEL_GUI_ICON_HEART}, {"Remove From Queue", SEL_GUI_ICON_MINUS}, {"Clear Queue", SEL_GUI_ICON_X},
    {"Move Up", SEL_GUI_ICON_CHEVRON_UP}, {"Move Down", SEL_GUI_ICON_CHEVRON_DOWN}, {"Help", SEL_GUI_ICON_ELLIPSIS},
    {"Settings", SEL_GUI_ICON_LAYOUT_GRID}, {"Open Playlist", SEL_GUI_ICON_LIST}, {"Rename Playlist", SEL_GUI_ICON_CHECK},
    {"Delete Playlist", SEL_GUI_ICON_X}, {"Clear Library", SEL_GUI_ICON_X},
    {"New Playlist", SEL_GUI_ICON_PLUS}, {"check", SEL_GUI_ICON_CHECK}, {"close", SEL_GUI_ICON_X},
    /* I973j: player bar speaker and subsong buttons pass the gui_icons.h file names directly */
    {"volume-2", SEL_GUI_ICON_VOLUME_2}, {"minus", SEL_GUI_ICON_MINUS}, {"plus", SEL_GUI_ICON_PLUS},
    /* breadcrumb */
    {"crumb", SEL_GUI_ICON_CHEVRON_RIGHT},
};

int mui_icon_id(const char* key) {
    size_t i;
    for (i = 0; key != NULL && i < sizeof k_icons / sizeof k_icons[0]; i++) {
        if (strcmp(k_icons[i].key, key) == 0) {
            g_resolved++;
            return k_icons[i].icon;
        }
    }
    return -1;
}

const char* mui_icon_name(int icon) {
    static const char* const k_names[] = {
#define X(id, file) file,
        SEL_GUI_ICONS(X)
#undef X
    };
    return icon >= 0 && (size_t)icon < sizeof k_names / sizeof k_names[0] ? k_names[icon] : NULL;
}

uint32_t mui_icon_resolved(void) {
    return g_resolved;
}
