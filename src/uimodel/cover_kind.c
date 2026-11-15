/* Cover kind table (I1399): kind to icon, tint token, label and ASCII glyph. Lives in uimodel so every UI links it,
 * Amiga LoadSeg segments included. */
#include <selecta/platform.h>
#include <selecta/gui/gui_icons.h>
#include <selecta/library_item.h>
#include <selecta/uimodel/tokens.h>

#include <ctype.h>
#include <string.h>

static const struct {
    uint32_t icon, token;
    const char *label, *glyph;
} cover_kinds[SEL_COVER_KIND_COUNT] = {
    {SEL_GUI_ICON_MUSIC, SEL_GUI_TOKEN_ACCENT, "Music", "[MU]"},
    {SEL_GUI_ICON_LIST_MUSIC, SEL_GUI_TOKEN_COVER_KIND_PLAYLIST, "Playlist", "[PL]"},
    {SEL_GUI_ICON_DISC, SEL_GUI_TOKEN_COVER_KIND_ALBUM, "Album", "[AL]"},
    {SEL_GUI_ICON_USER, SEL_GUI_TOKEN_COVER_KIND_ARTIST, "Artist", "[AR]"},
    {SEL_GUI_ICON_SERVER, SEL_GUI_TOKEN_COVER_KIND_DLNA, "DLNA", "[DL]"},
    {SEL_GUI_ICON_HARD_DRIVE, SEL_GUI_TOKEN_COVER_KIND_LOCAL, "Local Files", "[LF]"},
    {SEL_GUI_ICON_RADIO, SEL_GUI_TOKEN_COVER_KIND_RADIO, "Radio", "[RA]"},
    {SEL_GUI_ICON_PODCAST, SEL_GUI_TOKEN_COVER_KIND_PODCAST, "Podcast", "[PC]"},
    {SEL_GUI_ICON_GLOBE, SEL_GUI_TOKEN_COVER_KIND_MODULES_PL, "Modules.pl", "[MP]"},
    {SEL_GUI_ICON_LIBRARY, SEL_GUI_TOKEN_COVER_KIND_HIPPO, "HippoPlayer", "[HP]"},
    {SEL_GUI_ICON_FOLDER, SEL_GUI_TOKEN_COVER_KIND_ARCHIVE, "Archive", "[AC]"},
};

static const char* const cover_source_ids[] = {"local", "radio", "hippo", "dlna", "podcast", "modulespl"};
static const uint8_t cover_source_kinds[] = {SEL_COVER_KIND_LOCAL,   SEL_COVER_KIND_RADIO,       SEL_COVER_KIND_HIPPO,
                                             SEL_COVER_KIND_DLNA,    SEL_COVER_KIND_PODCAST,     SEL_COVER_KIND_MODULES_PL};

uint32_t sel_cover_kind_of_source(const char* id) {
    char low[24];
    size_t i;
    for (i = 0; i + 1 < sizeof low && id != NULL && id[i] != '\0'; i++) low[i] = (char)tolower((unsigned char)id[i]);
    low[i] = '\0';
    for (i = 0; i < sizeof cover_source_ids / sizeof cover_source_ids[0]; i++) {
        if (strcmp(low, cover_source_ids[i]) == 0) {
            return cover_source_kinds[i];
        }
    }
    return SEL_COVER_KIND_ARCHIVE;
}

uint32_t sel_cover_kind_of_item(uint32_t k, int is_album, int is_composer, const char* key) {
    if (k == (uint32_t)SEL_LIB_ITEM_PLAYLIST + 1u) return SEL_COVER_KIND_PLAYLIST;
    if (k == (uint32_t)SEL_LIB_ITEM_ALBUM + 1u || is_album) return SEL_COVER_KIND_ALBUM;
    if (k == (uint32_t)SEL_LIB_ITEM_ARTIST + 1u || k == (uint32_t)SEL_LIB_ITEM_COMPOSER + 1u || is_composer) return SEL_COVER_KIND_ARTIST;
    if (k == (uint32_t)SEL_LIB_ITEM_SOURCE + 1u) return sel_cover_kind_of_source(key);
    if (k == (uint32_t)SEL_LIB_ITEM_LOCAL + 1u) return SEL_COVER_KIND_LOCAL;
    return SEL_COVER_KIND_FORMAT;
}

uint32_t sel_cover_kind_icon(uint32_t kind) { return cover_kinds[kind < SEL_COVER_KIND_COUNT ? kind : 0u].icon; }
uint32_t sel_cover_kind_token(uint32_t kind) { return kind < SEL_COVER_KIND_COUNT ? cover_kinds[kind].token : 0u; }
const char* sel_cover_kind_label(uint32_t kind) { return cover_kinds[kind < SEL_COVER_KIND_COUNT ? kind : 0u].label; }
const char* sel_cover_kind_glyph(uint32_t kind) { return cover_kinds[kind < SEL_COVER_KIND_COUNT ? kind : 0u].glyph; }

uint32_t sel_cover_family_icon(uint32_t family) {
    switch (family) {
    case SEL_PLATFORM_FAMILY_MUSIC:
        return SEL_GUI_ICON_MUSIC;
    case SEL_PLATFORM_FAMILY_AMIGA:
    case SEL_PLATFORM_FAMILY_PC_TRACKER:
        return SEL_GUI_ICON_AUDIO_LINES;
    case SEL_PLATFORM_FAMILY_COMMODORE_64:
        return SEL_GUI_ICON_PIANO;
    default:
        return SEL_GUI_ICON_CPU;
    }
}
