/* Library modes, queue scopes and tile size steps (lifted out of plugins/ui/desktop for MUI-P8-3, P8-4, P8-12). One model for
 * every UI: the names a mode tab or Cycle shows, and the engine lines each choice sends. Pure C99. */
#ifndef SELECTA_UIMODEL_BROWSE_H
#define SELECTA_UIMODEL_BROWSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_LIB_MODES 6u
#define SEL_UIM_LIB_LINES 5u
#define SEL_UIM_LIB_LINE_MAX 40u
#define SEL_UIM_LIB_SOURCES 5u /* the Sources mode index */

/* Mode 0 Composers, 1 Tree, 2 Columns, 3 Files, 4 History, 5 Sources. NULL past the count. */
const char* sel_uim_libmode_name(uint32_t mode);
/* The engine lines entering `mode`, the Library window being `window_rows` high. Returns the line count. */
uint32_t sel_uim_libmode_lines(uint32_t mode, uint32_t window_rows, char out[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX]);

#define SEL_UIM_QUEUE_SCOPES 3u
/* Scope 0 library, 1 album, 2 playlist: the engine word, and the menu label ("Scope: Library"). NULL past the count. */
const char* sel_uim_scope_name(uint32_t i);
const char* sel_uim_scope_label(uint32_t i);
/* "SCOPE <name>" into out; returns the length, 0 for an index past the count. */
uint32_t sel_uim_scope_line(uint32_t i, char* out, uint32_t cap);

#define SEL_UIM_TILE_STEPS 5u
/* The grid and home shelf tile size steps in logical pixels (96..208); 0 past the count. */
int32_t sel_uim_tile_step(uint32_t i);

#ifdef __cplusplus
}
#endif

#endif
