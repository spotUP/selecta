/* Shared text of the Activity, Downloads and Extra Files panes (MUI-P8-7), the info line pieces (MUI-P8-9) and the remembered-state
 * pref lines (MUI-P8-11). Pure C99; read from sel_state, nothing is stored here. */
#ifndef SELECTA_UIMODEL_PANELS_H
#define SELECTA_UIMODEL_PANELS_H

#include <selecta/engine.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The engine counters the three panes and the info line show, gathered once from sel_sync_info, sel_scan_info, sel_engine_stats and sel_state. */
typedef struct sel_uim_net {
    uint32_t requests, cancels, bytes_read;
    uint32_t zip_bytes, listing_bytes, songlengths_bytes, stil_bytes;
    uint32_t changelogs_fetched, changelogs_missing;
    uint32_t lyric_source;
} sel_uim_net;
void sel_uim_net_fill(const sel_engine* e, sel_uim_net* out);

enum { SEL_UIM_PANEL_ACTIVITY = 0, SEL_UIM_PANEL_DOWNLOADS, SEL_UIM_PANEL_EXTRAS, SEL_UIM_PANEL_COUNT };
const char* sel_uim_panel_title(uint32_t panel); /* "Activity", "Downloads", "Extra Files"; NULL past the count */
uint32_t sel_uim_panel_row_count(uint32_t panel);
/* Row i of a panel as "Label: value" from the engine state; returns the length, 0 past the row count. */
uint32_t sel_uim_panel_row(uint32_t panel, const sel_uim_net* st, uint32_t i, char* out, uint32_t cap);

/* Info line (the DUI-P5-2 line text itself is not modelled yet): the four modes are chosen by the keys 7 8 9 0. */
#define SEL_UIM_INFO_MODES 4u
int32_t sel_uim_info_mode_for_key(char key); /* 0..3, -1 for any other key */
/* The scroll offset after one tick: unchanged when `no_scroll`, else advanced by one and wrapped at `text_len`. */
uint32_t sel_uim_info_scroll_next(uint32_t offset, uint32_t text_len, int no_scroll);
const char* sel_uim_info_ahi_text(int ahi_active); /* "AHI" or "" */
uint32_t sel_uim_info_downloaded_text(const sel_uim_net* st, char* out, uint32_t cap); /* "Downloaded <n> bytes" */
const char* sel_uim_kept_marker(int kept); /* column 0 of a list row: "*" or " " */

/* Remembered state: PREF_SET <key> "<value>" for the four items stored on close. */
enum { SEL_UIM_REM_GEOMETRY = 0, SEL_UIM_REM_PANES, SEL_UIM_REM_VISUALISER, SEL_UIM_REM_BROWSE, SEL_UIM_REM_COUNT };
const char* sel_uim_remember_key(uint32_t item);
uint32_t sel_uim_remember_line(uint32_t item, const char* value, char* out, uint32_t cap);

#ifdef __cplusplus
}
#endif
#endif
