/* MUI-P2-1 / MUI-P2-2: layout regions to group weights and the visible set per breakpoint. Pure C over the uimodel
 * layout tree (sel_uim_layout_compute, sel_uim_breakpoint_for), so the host tests what the Amiga build applies. */
#include <selecta/uimodel.h>

#include <string.h>

#include "mui_aa.h"
#include "mui_icons.h"
#include "mui_map.h"

static const char* const k_slot_names[MUI_SLOT_COUNT] = {"suggestion", "sidebar", "main", "detail", "player", "topbar-slot"};

const char* const mui_tool_labels[MUI_TOOL_BTNS] = {"Like", "Remove From Queue", "Clear Queue", "Move Up", "Move Down", "Help", "Settings",
                                                    "New Playlist", "New Folder", "Open Playlist", "Rename Playlist", "Delete Playlist", "Clear Library"};

uint32_t mui_tool_row_chars(uint32_t row) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if ((i < MUI_TOOL_ROW2 ? 0u : 1u) == row) {
            n += (uint32_t)strlen(mui_tool_labels[i]);
        }
    }
    return n;
}

uint32_t mui_tool_row_width(uint32_t row, int aspect_q8) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if ((i < MUI_TOOL_ROW2 ? 0u : 1u) == row) {
            n += mui_button_width(mui_tool_labels[i], MUI_TOOL_ICON_ONLY, aspect_q8);
        }
    }
    return n;
}

uint32_t mui_extra_row_width(int aspect_q8) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_EXTRA_COUNT; i++) {
        n += mui_button_width(mui_extra_label(i), MUI_EXTRA_ICON_ONLY, aspect_q8);
    }
    return n;
}

uint32_t mui_transport_row_width(uint32_t row, int aspect_q8) {
    uint32_t b, n = row == 0u ? MUI_SLIDER_MIN : 0u;
    for (b = 0; b < MUI_BTN_COUNT; b++) {
        if ((b < MUI_TRANSPORT_ROW1 ? 0u : 1u) != row) {
            continue;
        }
        uint32_t st, widest = 0;
        for (st = 0; st < 12u; st++) { /* play (2) x repeat (3) x random (2) */
            sel_uim_frame f;
            const char* l;
            memset(&f, 0, sizeof f);
            f.play = (st & 1u) != 0u ? SEL_PLAYING : SEL_STOPPED;
            f.repeat_mode = (st >> 1) % 3u;
            f.random_on = (st / 6u) != 0u;
            l = mui_transport_label(b, &f);
            if (l != NULL && mui_button_width(l, mui_transport_icon_only(b), aspect_q8) > widest) {
                widest = mui_button_width(l, mui_transport_icon_only(b), aspect_q8);
            }
        }
        n += widest;
    }
    return n;
}

void mui_layout_map(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier) {
    mui_layout_map_panel(m, width, height, tier, 0u);
}

/* I949c: the compact class is a bitmap-tier screen of 600-699 columns and at most MUI_COMPACT_MAX_ROWS rows; there the sidebar is
 * the labelled door rail. */
uint32_t mui_layout_compact(uint32_t width, uint32_t height, uint32_t tier) {
    return tier == SEL_UIM_TIER_BITMAP && sel_uim_breakpoint_for_tier(width, tier) == SEL_UIM_BP_ICON_SIDEBAR && width >= 600u && width < 700u &&
           height <= (uint32_t)MUI_COMPACT_MAX_ROWS;
}

uint32_t mui_layout_np_page(uint32_t width, uint32_t height, uint32_t tier) {
    return mui_layout_compact(width, height, tier) || sel_uim_breakpoint_for_tier(width, tier) != SEL_UIM_BP_THREE_COLUMN;
}

static uint32_t g_player_compact_builds;
uint32_t mui_player_band_compact_builds(void) { return g_player_compact_builds; }

uint32_t mui_art_height(uint32_t width, int aspect_q8) {
    return (uint32_t)mui_aa_rows((int)width, aspect_q8 > 0 ? aspect_q8 : 256);
}

static uint32_t g_player_bar_builds;
uint32_t mui_player_bar_builds(void) { return g_player_bar_builds; }

void mui_player_band(mui_player_band_t* out, uint32_t compact, int aspect_q8, uint32_t bar_w, uint32_t font_px) {
    mui_aa_metrics_t m;
    mui_aa_slider_t sl;
    uint32_t icon_box;
    memset(out, 0, sizeof *out);
    g_player_bar_builds++;
    mui_aa_metrics_aspect((int)font_px, aspect_q8, &m);
    mui_aa_slider_metrics((int)font_px, aspect_q8, &sl);
    out->band_n = 1;
    out->right_rows = 1;
    out->art_w = (uint32_t)MUI_PLAYER_ART_PX;
    out->art_h = mui_art_height(out->art_w, aspect_q8); /* square on screen: a tall pixel counts as its aspect in rows */
    icon_box = (uint32_t)(m.icon_h + 2 * m.pad_y);
    out->btn_px = icon_box > (uint32_t)m.height ? icon_box : (uint32_t)m.height;
    out->btn_w = (out->btn_px * (uint32_t)m.aspect_q8 + 128u) / 256u;
    out->btn_gap = (uint32_t)m.pad_x / 2u;
    out->gap = (uint32_t)m.pad_x;
    out->n_transport = MUI_BAR_TRANSPORT_N;
    out->show_left = bar_w >= (uint32_t)MUI_PLAYER_LEFT_MIN_W;
    out->sub_w = (uint32_t)MUI_PLAYER_SUB_LABEL_W + 2u * out->btn_w + 3u * out->btn_gap;
    out->track_h = (uint32_t)sl.track_h;
    out->slider_h = (uint32_t)sl.height;
    out->text_h = (uint32_t)m.height;
    out->height = (out->btn_px > out->art_h ? out->btn_px : out->art_h) + 2u * (uint32_t)MUI_PLAYER_COMPACT_PAD;
    if (compact) {
        g_player_compact_builds++;
        out->compact = 1;
        if (out->height > (uint32_t)MUI_PLAYER_COMPACT_MAX_H) {
            out->height = (uint32_t)MUI_PLAYER_COMPACT_MAX_H;
        }
    }
}

static uint32_t g_np_plans;
uint32_t mui_np_page_plans(void) { return g_np_plans; }

void mui_np_page_plan(const mui_aa_metrics_t* m, uint32_t main_w, mui_np_page_plan_t* out) {
    uint32_t next_head, lyr, info;
    memset(out, 0, sizeof *out);
    g_np_plans++;
    out->art_w = main_w / 3u < (uint32_t)MUI_DETAIL_ART_PX ? main_w / 3u : (uint32_t)MUI_DETAIL_ART_PX;
    out->art_h = mui_art_height(out->art_w, m->aspect_q8);
    out->text_rows = 4u;
    out->lyric_rows = 3u;
    out->next_rows = 3u;
    out->info_rows = 6u;
    out->text_h = (uint32_t)m->font_px;
    out->list_h = (uint32_t)m->height;
    out->heading_h = out->text_h + 2u;
    out->top_h = out->text_rows * out->text_h + 2u;
    if (out->art_h > out->top_h) {
        out->top_h = out->art_h;
    }
    out->tabs_h = (uint32_t)m->height;
    next_head = out->text_h;
    lyr = out->lyric_rows * out->list_h + next_head + out->next_rows * out->text_h;
    info = out->info_rows * out->list_h;
    out->page_h = lyr > info ? lyr : info;
    out->min_h = out->heading_h + out->top_h + out->tabs_h + out->page_h;
    if (out->min_h > (uint32_t)MUI_NP_PAGE_BUDGET_H && out->art_h > out->text_rows * out->text_h + 2u) {
        /* I960b: a square pixel makes the art taller than the text block; cap the art to what the budget leaves for the top block
         * (never below the text block) and shrink its width to stay square on screen. */
        uint32_t fixed = out->heading_h + out->tabs_h + out->page_h;
        uint32_t room = (uint32_t)MUI_NP_PAGE_BUDGET_H > fixed ? (uint32_t)MUI_NP_PAGE_BUDGET_H - fixed : 0u;
        uint32_t floor_h = out->text_rows * out->text_h + 2u;
        uint32_t ah = room > floor_h ? room : floor_h;
        if (ah < out->art_h) {
            uint32_t aw = (ah * (uint32_t)(m->aspect_q8 > 0 ? m->aspect_q8 : 256) + 128u) / 256u;
            out->art_w = aw < out->art_w ? aw : out->art_w;
            out->art_h = mui_art_height(out->art_w, m->aspect_q8);
            out->top_h = out->art_h > floor_h ? out->art_h : floor_h;
            out->min_h = out->heading_h + out->top_h + out->tabs_h + out->page_h;
        }
    }
}

uint32_t mui_player_centre_y(const mui_player_band_t* pb, uint32_t child_h) {
    return (pb->height - child_h) / 2u + child_h / 2u;
}

uint32_t mui_player_min_width(const mui_player_band_t* pb) {
    uint32_t trans = pb->n_transport * pb->btn_w + (pb->n_transport - 1u) * pb->btn_gap;
    /* groups: transport, clock, seek, length, speaker, volume = 6 children, 5 gaps */
    return trans + (uint32_t)MUI_PLAYER_CLOCK_W + (uint32_t)MUI_SLIDER_MIN + (uint32_t)MUI_PLAYER_CLOCK_W + pb->btn_w + (uint32_t)MUI_VOLUME_FIX_W + 5u * pb->gap;
}

/* I798 (T2, R3): the same map with the Now Playing column dropped when `detail_hidden`, through the layout the Mac computes
 * for its Hide Now Playing Panel (I619), so the main region takes the width on both. */
void mui_layout_map_panel(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t detail_hidden) {
    mui_layout_map_wide(m, width, height, tier, detail_hidden, 0u, SEL_UIM_LAYOUT_FULL);
}

static uint32_t g_mode_applies;
uint32_t mui_mode_applies(void) { return g_mode_applies; }

int mui_mode_from_pref(const char* key, const char* value, uint32_t* mode) {
    static const char* const names[4] = {"full", "window", "mini", "visual"};
    uint32_t i;
    if (key == NULL || value == NULL || strcmp(key, "ui.desktop.mode") != 0) {
        return 0;
    }
    for (i = 0; i < 4u; i++) {
        if (strcmp(value, names[i]) == 0) {
            *mode = i; /* the SEL_UIM_LAYOUT_* value of the same name (uimodel.h) */
            return 1;
        }
    }
    return 0;
}

/* I1173: the four shared layout actions (Full, Window, Mini, Visual; PREF_SET ui.desktop.mode) over the regions of the breakpoint.
 * Full keeps them. Window drops the Now Playing column (the desktop's small window). Mini keeps the player only. Visual keeps the
 * main region (the caller shows the Visualizer pane in it) and the player, so the visualizer fills the window. */
void mui_layout_map_mode(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t detail_hidden, uint32_t mode) {
    mui_layout_map_wide(m, width, height, tier, detail_hidden, 0u, mode);
}

/* I1166b + I1173: the one map builder. Expand Library (`side_wide`) and the layout mode are independent; relayout, the Now Playing
 * toggle and the Expand Library toggle pass both on, so neither resets the other. */
void mui_layout_map_wide(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t detail_hidden, uint32_t side_wide,
                         uint32_t mode) {
    sel_uim_layout l;
    uint32_t i, s;
    memset(m, 0, sizeof *m);
    /* I615d (design 8.1): the tier follows the screen as in the rich UI. A native planar screen is the bitmap tier (I674: a 640
     * wide window keeps a rail, no mini-layout suggestion, no top bar); an RTG screen is the default tier with the top bar. */
    sel_uim_layout_compute_tier_expand(&l, width, height, tier, detail_hidden != 0u || mode == SEL_UIM_LAYOUT_WINDOW, side_wide);
    m->side_wide = side_wide != 0u;
    m->detail_hidden = detail_hidden != 0u;
    m->mode = (uint8_t)mode;
    m->width = width;
    m->height = height;
    m->breakpoint = l.breakpoint;
    m->tier = tier;
    m->compact = (uint8_t)mui_layout_compact(width, height, tier);
    /* I615d (design 8.2): the top bar is its own field of the layout, not a region. */
    m->show[MUI_SLOT_TOPBAR] = l.topbar.h > 0u;
    m->weight_w[MUI_SLOT_TOPBAR] = width;
    m->weight_h[MUI_SLOT_TOPBAR] = l.topbar.h;
    for (i = 0; i < l.count; i++) {
        for (s = 0; s < MUI_SLOT_COUNT; s++) {
            if (strcmp(l.regions[i].name, k_slot_names[s]) == 0) {
                m->show[s] = 1;
                m->weight_w[s] = l.regions[i].w;
                m->weight_h[s] = l.regions[i].h;
            }
        }
    }
    if (mode == SEL_UIM_LAYOUT_MINI || mode == SEL_UIM_LAYOUT_VISUAL) {
        m->show[MUI_SLOT_TOPBAR] = 0;
        m->show[MUI_SLOT_SUGGESTION] = 0;
        m->show[MUI_SLOT_SIDEBAR] = 0;
        m->show[MUI_SLOT_DETAIL] = 0;
        m->show[MUI_SLOT_MAIN] = mode == SEL_UIM_LAYOUT_VISUAL;
        m->show[MUI_SLOT_PLAYER] = 1;
        m->weight_w[MUI_SLOT_MAIN] = width;
        if (m->weight_h[MUI_SLOT_PLAYER] == 0u) {
            m->weight_h[MUI_SLOT_PLAYER] = 1;
        }
    }
    if (mode != SEL_UIM_LAYOUT_FULL) {
        g_mode_applies++;
    }
    /* Balance objects sit between two shown panels of the body row. */
    m->balance_sidebar_main = m->show[MUI_SLOT_SIDEBAR] && m->show[MUI_SLOT_MAIN];
    m->balance_main_detail = m->show[MUI_SLOT_MAIN] && m->show[MUI_SLOT_DETAIL];
}

/* I798: the Hide / Show Now Playing button is offered where the Mac offers it (np_toggle_rect): on the three column breakpoint
 * with the top bar shown. */
uint32_t mui_np_toggle_shown(const mui_layout_map_t* m) {
    return m->show[MUI_SLOT_TOPBAR] && m->breakpoint == SEL_UIM_BP_THREE_COLUMN;
}

uint32_t mui_layout_relayout(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t* flips) {
    mui_layout_map_t next;
    uint32_t s, n = 0;
    mui_layout_map_wide(&next, width, height, tier, m->detail_hidden, m->side_wide, m->mode);
    if (next.breakpoint == m->breakpoint) {
        /* Same breakpoint: only the weights follow the new size, no object appears or disappears. */
        *m = next;
        if (flips != NULL) {
            *flips = 0;
        }
        return 0;
    }
    for (s = 0; s < MUI_SLOT_COUNT; s++) {
        n += m->show[s] != next.show[s];
    }
    *m = next;
    if (flips != NULL) {
        *flips = n;
    }
    return 1;
}
