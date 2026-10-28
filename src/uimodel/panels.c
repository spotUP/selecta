#include <selecta/uimodel/panels.h>

#include <stdio.h>

static uint32_t done(int n, uint32_t cap) { return n > 0 && (uint32_t)n < cap ? (uint32_t)n : 0u; }

static const char* const k_titles[SEL_UIM_PANEL_COUNT] = {"Activity", "Downloads", "Extra Files"};
static const uint32_t k_rows[SEL_UIM_PANEL_COUNT] = {3u, 4u, 3u};

const char* sel_uim_panel_title(uint32_t panel) { return panel < SEL_UIM_PANEL_COUNT ? k_titles[panel] : NULL; }

uint32_t sel_uim_panel_row_count(uint32_t panel) { return panel < SEL_UIM_PANEL_COUNT ? k_rows[panel] : 0u; }

uint32_t sel_uim_panel_row(uint32_t panel, const sel_uim_net* st, uint32_t i, char* out, uint32_t cap) {
    if (panel >= SEL_UIM_PANEL_COUNT || i >= k_rows[panel] || st == NULL) {
        return 0u;
    }
    if (panel == SEL_UIM_PANEL_ACTIVITY) {
        static const char* const l[3] = {"Requests", "Cancelled fetches", "Song bytes read"};
        uint32_t v[3];
        v[0] = st->requests;
        v[1] = st->cancels;
        v[2] = st->bytes_read;
        return done(snprintf(out, cap, "%s: %u", l[i], (unsigned)v[i]), cap);
    }
    if (panel == SEL_UIM_PANEL_DOWNLOADS) {
        static const char* const l[4] = {"Archive bytes", "Listing bytes", "Song length bytes", "Information file bytes"};
        uint32_t v[4];
        v[0] = st->zip_bytes;
        v[1] = st->listing_bytes;
        v[2] = st->songlengths_bytes;
        v[3] = st->stil_bytes;
        return done(snprintf(out, cap, "%s: %u", l[i], (unsigned)v[i]), cap);
    }
    {
        static const char* const l[3] = {"Lyrics source", "Changelogs fetched", "Changelogs missing"};
        uint32_t v[3];
        v[0] = st->lyric_source;
        v[1] = st->changelogs_fetched;
        v[2] = st->changelogs_missing;
        return done(snprintf(out, cap, "%s: %u", l[i], (unsigned)v[i]), cap);
    }
}

int32_t sel_uim_info_mode_for_key(char key) {
    return key == '7' ? 0 : key == '8' ? 1 : key == '9' ? 2 : key == '0' ? 3 : -1;
}

uint32_t sel_uim_info_scroll_next(uint32_t offset, uint32_t text_len, int no_scroll) {
    return no_scroll || text_len == 0u ? offset : (offset + 1u) % text_len;
}

const char* sel_uim_info_ahi_text(int ahi_active) { return ahi_active ? "AHI" : ""; }

uint32_t sel_uim_info_downloaded_text(const sel_uim_net* st, char* out, uint32_t cap) {
    uint32_t total = st->zip_bytes + st->listing_bytes + st->songlengths_bytes + st->stil_bytes;
    return done(snprintf(out, cap, "Downloaded %u bytes", (unsigned)total), cap);
}

const char* sel_uim_kept_marker(int kept) { return kept ? "*" : " "; }

static const char* const k_rem_keys[SEL_UIM_REM_COUNT] = {"ui.mui.geometry", "ui.mui.panes", "ui.desktop.viz", "ui.mui.browse_view"};

const char* sel_uim_remember_key(uint32_t item) { return item < SEL_UIM_REM_COUNT ? k_rem_keys[item] : NULL; }

uint32_t sel_uim_remember_line(uint32_t item, const char* value, char* out, uint32_t cap) {
    return item < SEL_UIM_REM_COUNT ? done(snprintf(out, cap, "PREF_SET %s \"%s\"", k_rem_keys[item], value), cap) : 0u;
}

void sel_uim_net_fill(const sel_engine* e, sel_uim_net* out) {
    sel_sync_info sync;
    sel_scan_info scan;
    sel_engine_stats stats;
    sel_engine_sync_info(e, &sync);
    sel_engine_scan_info(e, &scan);
    sel_engine_stats_get(e, &stats);
    out->requests = sync.net_requests;
    out->cancels = stats.net_cancels;
    out->bytes_read = scan.bytes_read;
    out->zip_bytes = sync.net_zip_bytes;
    out->listing_bytes = sync.net_listing_bytes;
    out->songlengths_bytes = sync.net_songlengths_bytes;
    out->stil_bytes = sync.net_stil_bytes;
    out->changelogs_fetched = sync.net_changelogs_fetched;
    out->changelogs_missing = sync.net_changelogs_missing;
    out->lyric_source = sel_engine_state(e)->lyric_source;
}
