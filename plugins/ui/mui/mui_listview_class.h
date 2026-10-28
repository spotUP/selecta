/* Selecta_Listview (MUI-P4-6, I558): Listview subclass that turns the right Amiga + 1..n Song menu keys, pressed with the
 * pointer over the list, into the same ReturnID the window Menustrip sends. MUI 3.8 only. Amiga only. */
#ifndef MUI_LISTVIEW_CLASS_H
#define MUI_LISTVIEW_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

/* Creates the class (once) and a Listview of it; NULL when MUI refuses the class. Takes the MUIA_Listview_List tag value,
 * the ReturnID of Song menu item 0 and the item count (keys 1..sm_count). */
Object* mui_listview_new(Object* list, ULONG sm_base, ULONG sm_count);
/* Deletes the class; call after the application object is disposed. */
void mui_listview_class_free(void);

#endif
