/* The platform independent half of the Amiga folder dialog (I531): turns what an asl.library drawers-only requester
 * answered into the SEL_GUI_IN_DROP event the SDL backend delivers for its folder pick (backend_sdl.c queue_drop), which
 * desktop_ui.c answers with LIBRARY_SCAN. The host unit test builds this file alone, it needs no Amiga headers. */
#ifndef SELECTA_DESKTOP_AMIGA_FOLDER_PICK_H
#define SELECTA_DESKTOP_AMIGA_FOLDER_PICK_H

#include <selecta/gui/gui_input.h>

#include <stddef.h>

/* Joins drawer and file the way dos.library AddPart does: a drawer ending in ':' or '/' gets no extra '/', an empty
 * file leaves the drawer alone. 0 when drawer is NULL or empty (a cancelled requester) or the result does not fit cap. */
int desk_amiga_folder_path(const char* drawer, const char* file, char* out, size_t cap);

/* Fills ev with the drop event of the chosen folder; 0 and ev untouched on cancel (drawer NULL or empty). */
int desk_amiga_folder_event(const char* drawer, const char* file, sel_gui_input* ev);

#endif
