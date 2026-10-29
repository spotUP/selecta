/* Design tokens of the desktop UI (plan DUI-P1-10, M4 plan section 2): one X-macro lists every token with its file
 * key, default and range. The tokens live inside txt_theme (theme.h), so the one theme parser reads them. Integers
 * only: colours are 0xRRGGBB, sizes are pixels. Pure C99. */
#ifndef SELECTA_UIMODEL_TOKENS_H
#define SELECTA_UIMODEL_TOKENS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* X(id, file key, default, minimum, maximum) */
#define SEL_GUI_TOKENS(X)                                \
    X(ACCENT, "gui.accent", 0x3B82F6u, 0u, 0xFFFFFFu)    \
    X(SURFACE, "gui.surface", 0x1E1E24u, 0u, 0xFFFFFFu)  \
    X(TEXT, "gui.text", 0xC8C8D0u, 0u, 0xFFFFFFu)        \
    X(RADIUS, "gui.radius", 6u, 0u, 32u)                 \
    X(SPACING, "gui.spacing", 8u, 0u, 64u)               \
    X(BACKGROUND, "gui.background", 0x121214u, 0u, 0xFFFFFFu)        \
    X(SURFACE_RAISED, "gui.surface_raised", 0x2A2A32u, 0u, 0xFFFFFFu) \
    X(SURFACE_HOVER, "gui.surface_hover", 0x34343Eu, 0u, 0xFFFFFFu)   \
    X(SURFACE_PRESSED, "gui.surface_pressed", 0x3E3E4Au, 0u, 0xFFFFFFu) \
    X(TEXT_MUTED, "gui.text_muted", 0xA0A0ACu, 0u, 0xFFFFFFu)        \
    X(TEXT_DISABLED, "gui.text_disabled", 0x62626Eu, 0u, 0xFFFFFFu)  \
    X(ACCENT_TEXT, "gui.accent_text", 0xE4E4EAu, 0u, 0xFFFFFFu)      \
    X(DIVIDER, "gui.divider", 0x2C2C34u, 0u, 0xFFFFFFu)              \
    X(FOCUS_RING, "gui.focus_ring", 0x93C5FDu, 0u, 0xFFFFFFu)        \
    X(ERROR, "gui.error", 0xEF4444u, 0u, 0xFFFFFFu)                  \
    X(WARNING, "gui.warning", 0xF59E0Bu, 0u, 0xFFFFFFu)              \
    X(SUCCESS, "gui.success", 0x22C55Eu, 0u, 0xFFFFFFu)              \
    X(SCRIM, "gui.scrim", 0x000000u, 0u, 0xFFFFFFu)                  \
    X(RADIUS_SMALL, "gui.radius_small", 4u, 0u, 32u)                 \
    X(RADIUS_LARGE, "gui.radius_large", 8u, 0u, 32u)                 \
    X(FRAME, "gui.frame", 0x09090Bu, 0u, 0xFFFFFFu)                  \
    X(PANEL, "gui.panel", 0x141418u, 0u, 0xFFFFFFu)                  \
    X(HEADER_TINT, "gui.header_tint", 0x2B2350u, 0u, 0xFFFFFFu)      \
    X(TRACK, "gui.track", 0x3A3A44u, 0u, 0xFFFFFFu)                  \
    X(HOVER_LIGHT, "gui.hover_light", 0xE4E4EAu, 0u, 0xFFFFFFu)      \
    X(TILE_PAD, "gui.tile_pad", 10u, 0u, 64u)                        \
    X(COVER_AMIGA, "gui.cover.amiga", 0xB4390Au, 0u, 0xFFFFFFu)      \
    X(COVER_COMMODORE_64, "gui.cover.commodore_64", 0x4338CAu, 0u, 0xFFFFFFu) \
    X(COVER_ATARI, "gui.cover.atari", 0x14703Au, 0u, 0xFFFFFFu)      \
    X(COVER_ZX_SPECTRUM, "gui.cover.zx_spectrum", 0xB91C1Cu, 0u, 0xFFFFFFu) \
    X(COVER_PC_TRACKER, "gui.cover.pc_tracker", 0x0B6A82u, 0u, 0xFFFFFFu) \
    X(COVER_NES, "gui.cover.nes", 0x9D174Du, 0u, 0xFFFFFFu)          \
    X(COVER_GAME_BOY, "gui.cover.game_boy", 0x3F6212u, 0u, 0xFFFFFFu) \
    X(COVER_SNES, "gui.cover.snes", 0x6D28D9u, 0u, 0xFFFFFFu)        \
    X(COVER_PC_ENGINE, "gui.cover.pc_engine", 0x854D0Eu, 0u, 0xFFFFFFu) \
    X(COVER_MSX, "gui.cover.msx", 0x475569u, 0u, 0xFFFFFFu)          \
    X(COVER_MEGA_DRIVE, "gui.cover.mega_drive", 0x1D4ED8u, 0u, 0xFFFFFFu) \
    X(COVER_CONSOLE, "gui.cover.console", 0x52525Bu, 0u, 0xFFFFFFu)

/* Layer 1 (D-DUI-9): base tokens. A theme swaps these and the role tokens above follow. X(id, file key, default, min,
 * max). A value may be written as `@gui.base.<name>` (a reference to another base token). Widgets read roles only. */
#define SEL_GUI_BASE_TOKENS(X)                                  \
    X(GREY_0, "gui.base.grey_0", 0x000000u, 0u, 0xFFFFFFu)      \
    X(GREY_1, "gui.base.grey_1", 0x121214u, 0u, 0xFFFFFFu)      \
    X(GREY_2, "gui.base.grey_2", 0x1E1E24u, 0u, 0xFFFFFFu)      \
    X(GREY_3, "gui.base.grey_3", 0x2A2A32u, 0u, 0xFFFFFFu)      \
    X(GREY_4, "gui.base.grey_4", 0x2C2C34u, 0u, 0xFFFFFFu)      \
    X(GREY_5, "gui.base.grey_5", 0x34343Eu, 0u, 0xFFFFFFu)      \
    X(GREY_6, "gui.base.grey_6", 0x3E3E4Au, 0u, 0xFFFFFFu)      \
    X(GREY_7, "gui.base.grey_7", 0x62626Eu, 0u, 0xFFFFFFu)      \
    X(GREY_8, "gui.base.grey_8", 0xA0A0ACu, 0u, 0xFFFFFFu)      \
    X(GREY_9, "gui.base.grey_9", 0xC8C8D0u, 0u, 0xFFFFFFu)      \
    X(GREY_10, "gui.base.grey_10", 0xE4E4EAu, 0u, 0xFFFFFFu)    \
    X(ACCENT_0, "gui.base.accent_0", 0x3B82F6u, 0u, 0xFFFFFFu)  \
    X(ACCENT_1, "gui.base.accent_1", 0x93C5FDu, 0u, 0xFFFFFFu)  \
    X(ERROR_0, "gui.base.error_0", 0xEF4444u, 0u, 0xFFFFFFu)    \
    X(WARNING_0, "gui.base.warning_0", 0xF59E0Bu, 0u, 0xFFFFFFu) \
    X(SUCCESS_0, "gui.base.success_0", 0x22C55Eu, 0u, 0xFFFFFFu) \
    X(SPACING_1, "gui.base.spacing_1", 4u, 0u, 64u)             \
    X(SPACING_2, "gui.base.spacing_2", 8u, 0u, 64u)             \
    X(RADIUS_1, "gui.base.radius_1", 4u, 0u, 32u)               \
    X(RADIUS_2, "gui.base.radius_2", 6u, 0u, 32u)               \
    X(RADIUS_3, "gui.base.radius_3", 8u, 0u, 32u)               \
    X(FRAME_0, "gui.base.frame_0", 0x09090Bu, 0u, 0xFFFFFFu)    \
    X(PANEL_0, "gui.base.panel_0", 0x141418u, 0u, 0xFFFFFFu)    \
    X(TINT_0, "gui.base.tint_0", 0x2B2350u, 0u, 0xFFFFFFu)      \
    X(TRACK_0, "gui.base.track_0", 0x3A3A44u, 0u, 0xFFFFFFu)    \
    X(SPACING_3, "gui.base.spacing_3", 10u, 0u, 64u)

typedef enum sel_gui_base {
#define X(id, name, def, lo, hi) SEL_GUI_BASE_##id,
    SEL_GUI_BASE_TOKENS(X)
#undef X
    SEL_GUI_BASE_COUNT
} sel_gui_base;

/* Default references of the Selecta Dark preset: X(role id, base id). Every role follows a base token by default. */
#define SEL_GUI_TOKEN_REFS(X)            \
    X(ACCENT, ACCENT_0)                  \
    X(SURFACE, GREY_2)                   \
    X(TEXT, GREY_9)                      \
    X(RADIUS, RADIUS_2)                  \
    X(SPACING, SPACING_2)                \
    X(BACKGROUND, GREY_1)                \
    X(SURFACE_RAISED, GREY_3)            \
    X(SURFACE_HOVER, GREY_5)             \
    X(SURFACE_PRESSED, GREY_6)           \
    X(TEXT_MUTED, GREY_8)                \
    X(TEXT_DISABLED, GREY_7)             \
    X(ACCENT_TEXT, GREY_10)              \
    X(DIVIDER, GREY_4)                   \
    X(FOCUS_RING, ACCENT_1)              \
    X(ERROR, ERROR_0)                    \
    X(WARNING, WARNING_0)                \
    X(SUCCESS, SUCCESS_0)                \
    X(SCRIM, GREY_0)                     \
    X(RADIUS_SMALL, RADIUS_1)            \
    X(RADIUS_LARGE, RADIUS_3)            \
    X(FRAME, FRAME_0)                    \
    X(PANEL, PANEL_0)                    \
    X(HEADER_TINT, TINT_0)               \
    X(TRACK, TRACK_0)                    \
    X(HOVER_LIGHT, GREY_10)              \
    X(TILE_PAD, SPACING_3)

typedef enum sel_gui_token {
#define X(id, name, def, lo, hi) SEL_GUI_TOKEN_##id,
    SEL_GUI_TOKENS(X)
#undef X
    SEL_GUI_TOKEN_COUNT
} sel_gui_token;

const char* sel_gui_token_name(sel_gui_token t);
uint32_t sel_gui_token_default(sel_gui_token t);
uint32_t sel_gui_token_min(sel_gui_token t);
uint32_t sel_gui_token_max(sel_gui_token t);
/* Index of the token with this file key, or -1. */
int sel_gui_token_find(const char* name, uint32_t len);
const char* sel_gui_base_name(sel_gui_base b);
uint32_t sel_gui_base_default(sel_gui_base b);
uint32_t sel_gui_base_min(sel_gui_base b);
uint32_t sel_gui_base_max(sel_gui_base b);
/* Index of the base token with this file key, or -1. */
int sel_gui_base_find(const char* name, uint32_t len);
/* Base token a role follows in the Selecta Dark preset. */
int sel_gui_token_default_ref(sel_gui_token t);
/* Bit mask (1 << token) of the tokens that differ between two token arrays. */
uint64_t sel_gui_tokens_diff(const uint32_t* a, const uint32_t* b);

#ifdef __cplusplus
}
#endif
#endif
