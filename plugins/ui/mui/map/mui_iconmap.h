/* I685g (design 2026-11-06-mui-mcc-package.md section 6): the Selecta side of the icon names. The class package addresses icons
 * by Lucide name only (mui_icons.h); the sel_gui_icon enum and the button key table live here. Pure C99. */
#ifndef SELECTA_MUI_ICONMAP_H
#define SELECTA_MUI_ICONMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Icon id (a sel_gui_icon value) for a button key: a transport or extra action id ("play", "pitch_up"), a sidebar door page
 * ("home") or a toolbar label ("Clear Queue"); the breadcrumb button uses "crumb". -1 when the key has no icon. Counts each
 * resolved lookup (mui_icon_resolved). */
int mui_icon_id(const char* key);
/* Number of successful mui_icon_id lookups since start (sentinel for the build path test). */
uint32_t mui_icon_resolved(void);
/* Lucide name of a sel_gui_icon value (the file name in include/selecta/gui/gui_icons.h), NULL for a value out of range. */
const char* mui_icon_name(int icon);

#ifdef __cplusplus
}
#endif
#endif
