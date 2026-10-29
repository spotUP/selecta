/* I651: button icon lookup and palette. See mui_icons.h. */
#include "mui_icons.h"
#include "mui_aa.h"

#include <string.h>

#include <selecta/gui/gui_icons.h>
#include <selecta/uimodel/tokens.h>

static uint32_t g_resolved;
static uint32_t g_lookups[8];

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
    {"home", SEL_GUI_ICON_HOUSE}, {"search", SEL_GUI_ICON_SEARCH}, {"library", SEL_GUI_ICON_LIBRARY}, {"playlists", SEL_GUI_ICON_MUSIC},
    {"queue", SEL_GUI_ICON_LIST}, {"nowplaying", SEL_GUI_ICON_DISC}, {"songinfo", SEL_GUI_ICON_USER},
    /* toolbar (mui_tool_labels) */
    {"Like", SEL_GUI_ICON_HEART}, {"Remove From Queue", SEL_GUI_ICON_MINUS}, {"Clear Queue", SEL_GUI_ICON_X},
    {"Move Up", SEL_GUI_ICON_CHEVRON_UP}, {"Move Down", SEL_GUI_ICON_CHEVRON_DOWN}, {"Help", SEL_GUI_ICON_ELLIPSIS},
    {"Settings", SEL_GUI_ICON_LAYOUT_GRID}, {"Open Playlist", SEL_GUI_ICON_LIST}, {"Rename Playlist", SEL_GUI_ICON_CHECK},
    {"Delete Playlist", SEL_GUI_ICON_X}, {"Clear Library", SEL_GUI_ICON_X},
    {"New Playlist", SEL_GUI_ICON_PLUS}, {"check", SEL_GUI_ICON_CHECK},
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

const uint8_t* mui_icon_body(int icon, int px) {
    uint32_t z;
    if (icon < 0 || (uint32_t)icon >= mui_icon_body_count) {
        return (const uint8_t*)0;
    }
    for (z = 0; z < mui_icon_size_count; z++) {
        if ((int)mui_icon_sizes[z] == px) {
            g_lookups[z]++;
            return mui_icon_sets[z][icon];
        }
    }
    return (const uint8_t*)0;
}

uint32_t mui_icon_body_lookups(int px) {
    uint32_t z;
    for (z = 0; z < mui_icon_size_count; z++) {
        if ((int)mui_icon_sizes[z] == px) {
            return g_lookups[z];
        }
    }
    return 0;
}

int mui_model_icon_px(void) {
    mui_aa_metrics_t m;
    mui_aa_metrics(mui_aa_font_px(512, 0), &m);
    return m.icon_px;
}

uint32_t mui_icon_resolved(void) {
    return g_resolved;
}

static uint32_t spread(uint32_t v) {
    return (v & 0xFFu) * 0x01010101u;
}

void mui_icon_palette(const uint32_t* tokens, int themed, uint32_t out[12]) {
    uint32_t face = themed ? tokens[SEL_GUI_TOKEN_SURFACE_RAISED] : 0xAAAAAAu;
    uint32_t ink = themed ? tokens[SEL_GUI_TOKEN_TEXT] : 0x000000u;
    uint32_t pen, ch;
    for (pen = 0; pen < 4; pen++) {
        for (ch = 0; ch < 3; ch++) {
            uint32_t sh = 16u - 8u * ch, a = (face >> sh) & 0xFFu, b = (ink >> sh) & 0xFFu;
            out[pen * 3 + ch] = spread((a * (3u - pen) + b * pen) / 3u); /* thirds from face to ink; pen 0 is transparent */
        }
    }
}

uint32_t mui_button_width(const char* label, int icon_only) {
    uint32_t w = 2u * MUI_BTN_PAD_X + MUI_BTN_FRAME + (uint32_t)mui_model_icon_px();
    if (!icon_only && label != NULL) {
        w += MUI_BTN_ICON_GAP + (uint32_t)strlen(label) * MUI_FONT_W;
    }
    return w;
}
