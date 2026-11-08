/* Selecta_Keys (I1165, I1171): an invisible Rectangle subclass that owns the window key table. MUI 3.8 only. Amiga only. */
#ifndef MUI_KEYS_CLASS_H
#define MUI_KEYS_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

/* Creates the class (once) and one instance; NULL when MUI refuses the class. key_base: ReturnID of action key row 0
 * (mui_key_row_at index added); door_base / pane_base: the ReturnIDs the sidebar door and main pane buttons already send.
 */
Object* mui_keys_new(ULONG key_base, ULONG door_base, ULONG pane_base);
/* Sets the Search string gadget and the results list; call once the window tree exists (argument order inside the tree is unspecified). */
void mui_keys_targets(Object* search, Object* list);
/* Deletes the class; call after the application object is disposed. */
void mui_keys_class_free(void);

#endif
