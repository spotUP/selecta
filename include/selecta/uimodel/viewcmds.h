/* Engine lines of the view buttons and the Amiga runtime prefs (lifted out of plugins/ui/desktop for MUI-P8-5, P8-14): one model
 * for every UI. Pure C99. */
#ifndef SELECTA_UIMODEL_VIEWCMDS_H
#define SELECTA_UIMODEL_VIEWCMDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_CMD_MAX 160u

/* Playlists view buttons: 0 New Folder, 1 New Playlist. `name` empty or NULL uses the default name. Returns the line length, 0
 * for a bad button. */
uint32_t sel_uim_plbtn_line(uint32_t button, const char* name, uint32_t folder, char* out, uint32_t cap);

/* Song Information buttons: 0 Mark Wrong, 1 Skip Song, 2 Save Note (`note` may be empty or NULL). */
const char* sel_uim_flag_label(uint32_t button);
uint32_t sel_uim_flag_line(uint32_t button, const char* note, char* out, uint32_t cap);

/* Amiga runtime prefs (MUI-P8-14). */
#define SEL_UIM_PRIORITY_MIN (-5)
#define SEL_UIM_PRIORITY_MAX 5
int32_t sel_uim_priority_clamp(int32_t v);
uint32_t sel_uim_hotkeys_line(int enabled, char* out, uint32_t cap);
uint32_t sel_uim_priority_line(int32_t v, char* out, uint32_t cap);
uint32_t sel_uim_groupfile_line(const char* path, char* out, uint32_t cap);

#ifdef __cplusplus
}
#endif

#endif
