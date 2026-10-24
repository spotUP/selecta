/* Widget set of the desktop UI (DUI-P3-1, plan 2.3). Each widget is a plain record (kind, rectangle, state bits,
 * label, values) drawn by sel_gui_widget_draw() with the rasterizer, the text layout and the icon atlas, from a style
 * the plugin fills out of the gui.* design tokens: no colour, radius or spacing is fixed in this module. Integer and
 * Q16.16 only. Hit testing, focus order and events belong to the layout layer (DUI-P3-2); this module only draws. */
#ifndef SELECTA_GUI_WIDGET_H
#define SELECTA_GUI_WIDGET_H

#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_icons.h>
#include <selecta/gui/gui_text.h>

#include <stdint.h>

/* The one list of widget kinds: X(ID, "name"). */
#define SEL_GUI_WIDGETS(X)       \
    X(LABEL, "label")            \
    X(ICON, "icon")              \
    X(ICON_BUTTON, "icon_button") \
    X(TEXT_BUTTON, "text_button") \
    X(TOGGLE, "toggle")          \
    X(SLIDER, "slider")          \
    X(PROGRESS, "progress")      \
    X(VIRTUAL_LIST, "virtual_list") \
    X(TABLE_HEADER, "table_header") \
    X(TILE_GRID, "tile_grid")    \
    X(COVER_FLOW, "cover_flow")  \
    X(TABS, "tabs")              \
    X(SIDEBAR_LIST, "sidebar_list") \
    X(SPLIT_PANE, "split_pane")  \
    X(SCROLL_BAR, "scroll_bar")  \
    X(TEXT_FIELD, "text_field")  \
    X(MENU, "menu")              \
    X(TOOLTIP, "tooltip")        \
    X(TOAST, "toast")            \
    X(MODAL_DIALOG, "modal_dialog") \
    X(SCRIM, "scrim")            \
    X(ARTWORK, "artwork")        \
    X(VIZ_SURFACE, "viz_surface") \
    X(SEGMENTED, "segmented")    \
    X(CHECKBOX, "checkbox")      \
    X(RADIO, "radio")            \
    X(DROPDOWN, "dropdown")

typedef enum sel_gui_widget_kind {
#define X(id, name) SEL_GUI_WIDGET_##id,
    SEL_GUI_WIDGETS(X)
#undef X
    SEL_GUI_WIDGET_KIND_COUNT
} sel_gui_widget_kind;

/* State bits. */
#define SEL_GUI_WS_HOVER 1u
#define SEL_GUI_WS_PRESSED 2u
#define SEL_GUI_WS_FOCUS 4u
#define SEL_GUI_WS_SELECTED 8u /* on / checked / current / primary, by kind */
#define SEL_GUI_WS_DISABLED 16u

#define SEL_GUI_Q16_ONE 65536

typedef struct sel_gui_widget {
    uint32_t kind; /* sel_gui_widget_kind */
    sel_gui_rect r;
    uint32_t state;
    const char* label;
    int32_t icon;  /* sel_gui_icon, or -1 for none; sidebar rows use item_icons[row] when set, else icon + row */
    int32_t value; /* Q16.16: slider, progress, scroll position, split fraction, scrim alpha, viz phase */
    int32_t extent; /* Q16.16: scroll thumb length as a fraction of the track */
    const char* const* items;
    uint32_t item_n;
    const int32_t* item_icons; /* sidebar: one sel_gui_icon per item, or NULL */
    int32_t sel; /* current item */
    int32_t top; /* first visible row, artwork tone */
} sel_gui_widget;

/* Everything a widget draws with. Colours are 0xAARRGGBB words taken from the gui.* tokens. */
typedef struct sel_gui_style {
    uint32_t background, surface, raised, hover, pressed;
    uint32_t text, muted, disabled, accent, accent_text, divider, focus;
    uint32_t error, warning, success, scrim;
    int32_t radius_small, radius, radius_large, spacing;
    const sel_gui_atlas* body;
    const sel_gui_atlas* semibold;
    const sel_gui_atlas* caption;
    const sel_gui_icons* icons;
    const sel_gui_shadow* shadow;
} sel_gui_style;

typedef struct sel_gui_widget_stats {
    uint32_t draws[SEL_GUI_WIDGET_KIND_COUNT]; /* per kind: the STAT_WIDGET_DRAWS breakdown */
    sel_gui_text_stats text;
} sel_gui_widget_stats;

const char* sel_gui_widget_kind_name(uint32_t kind);

/* Draws one widget; returns 0, or -1 for an unknown kind. */
int sel_gui_widget_draw(sel_gui_canvas* c, const sel_gui_style* s, const sel_gui_widget* w,
                        sel_gui_widget_stats* st);

#endif
