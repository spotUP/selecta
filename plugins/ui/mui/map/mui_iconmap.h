/* I685g (design 2026-11-06-mui-mcc-package.md section 6): the Selecta side of the icon names. The class package addresses icons
 * by Lucide name only (mui_icons.h); the sel_gui_icon enum and the button key table live here. Pure C99. */
#ifndef SELECTA_MUI_ICONMAP_H
#define SELECTA_MUI_ICONMAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Lucide name of a sel_gui_icon value (the file name in include/selecta/gui/gui_icons.h), NULL for a value out of range. */
const char* mui_icon_name(int icon);

#ifdef __cplusplus
}
#endif
#endif
