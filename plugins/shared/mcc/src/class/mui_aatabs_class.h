/* Selecta_AATabs (I667j, design 2026-10-28-mui-custom-classes.md section 3.9): MUI 3.8 Group subclass, a row of AAButton tab captions
 * (role TAB: TEXT_MUTED idle, TEXT active) with a 2 px ACCENT underline under the active one. Replaces a stock Register group:
 * the pages are a plain page Group the caller switches with a notification on AATab_Active. Amiga only; click, key and underline
 * maths are in map/mui_aa.c and tested on the host (mui-tabs). */
#ifndef MUI_AATABS_CLASS_H
#define MUI_AATABS_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>

#define AATab_Active (TAG_USER + 0x53410301UL) /* LONG: index of the active tab; notifies like MUIA_Group_ActivePage */
#define MUI_AATABS_MAX 8

typedef struct mui_aatabs_spec {
    const char* const* captions; /* NULL terminated, at most MUI_AATABS_MAX; the strings must outlive the object */
    int active;                  /* initially active tab */
} mui_aatabs_spec;

Object* mui_aatabs_new(const mui_aatabs_spec* spec);
/* Tab strips constructed since start (sentinel: equals the number of tab sites the window build made). */
uint32_t mui_aatabs_made(void);
void mui_aatabs_class_free(void);

#endif
