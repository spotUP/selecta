/* I731 (plan 2026-10-31-I731-mui-flat-tooltips.md section 2.2): the flat MUI tooltip. MUI never receives a control's help text:
 * the three tipped Selecta_AA classes (AABtn, AASlider, AAField) take MUIA_ShortHelp out of their tag lists, so MUI's own 3D bubble
 * never opens. After 600 ms of rest this module opens a borderless Intuition window drawn with mui_aa_draw (flat rounded card). The
 * plan, placement and hover state are pure and live in map/mui_aatip.c (mui_aa_tip_*). */
#ifndef MUI_AATIP_H
#define MUI_AATIP_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <stdint.h>

#include "mui_aa_draw.h"

typedef struct aatip_slot {
    const char* text;
    int side; /* MUI_AA_TIP_* preferred side */
    int in;   /* the pointer is inside the owner (ENTER / LEAVE edges) */
} aatip_slot;

#define AATIP_M_FIRE (TAG_USER | 0x5E1A0731UL) /* private method the timer node sends to the owner */
#define AATIP_A_Side (TAG_USER | 0x5E1A0732UL) /* I/S: preferred side, MUI_AA_TIP_* */

/* OM_NEW / OM_SET: take MUIA_ShortHelp and AATIP_A_Side out of the tag list, call the super, restore the tags, then store the values
 * in the aatip_slot at byte offset slot_off of the class instance data (of the new object on OM_NEW). Returns the super's result. */
ULONG aatip_super_set(struct IClass* cl, Object* obj, struct opSet* msg, ULONG slot_off);
/* OM_GET MUIA_ShortHelp -> t->text. Returns 1 when answered. */
int aatip_get(struct opGet* msg, const aatip_slot* t);
/* Call first in the owner's MUIM_HandleEvent; never eats. in = pointer inside the owner. */
void aatip_event(Object* obj, aatip_slot* t, const struct IntuiMessage* im, int in);
/* AATIP_M_FIRE: the timer ran out on obj. */
ULONG aatip_fire(Object* obj, aa_ctx* aa, const aatip_slot* t);
/* MUIM_Hide / MUIM_Cleanup of the owner, retheme, window and screen close: drop the tip if obj owns it (obj NULL = any owner). */
void aatip_drop(Object* obj);
uint32_t aatip_shows(void); /* tips opened, for the rig log line */

#endif
