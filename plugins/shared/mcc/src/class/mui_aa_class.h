#ifndef MUI_AA_CLASS_H
#define MUI_AA_CLASS_H

/* The static/shared switch of the class package (I685i, design 2026-11-06-mui-mcc-package.md section 3.2). Every Selecta_AA* class
 * file describes its class once, in an aa_class_desc: superclass name, instance size, dispatcher, and the file's class pointer.
 * The two places where the static build and the .mcc build differ are AA_CLASS_CREATE (below) and lib/classheader.c, which reads
 * the description and creates the class once in the library init. Dispatcher, draw, layout and input code are the same in both. */
#include <stdint.h>

#include <exec/types.h>
#include <libraries/mui.h>

struct aa_class_desc {
    const char* super;                 /* MUIC_Area, MUIC_Group, ... */
    ULONG size;                        /* sizeof the instance data */
    ULONG (*dispatcher)(void);         /* the asm entry stub of the class (a0 class, a2 object, a1 message) */
    struct MUI_CustomClass** slot;     /* the class file's own class pointer */
};

#ifdef UP_MCC_LIBRARY
/* .mcc build: the library init created the class with the library base as first argument and stored it in *slot. */
#define AA_CLASS_CREATE(desc) (*(desc)->slot)
#else
/* Static build: the class is created on first use with a NULL base. */
#define AA_CLASS_CREATE(desc) MUI_CreateCustomClass(NULL, (char*)(desc)->super, NULL, (desc)->size, (APTR)(uintptr_t)(desc)->dispatcher)
#endif

extern const struct aa_class_desc aabtn_class_desc, aatext_class_desc, aapanel_class_desc, aatabs_class_desc, aabal_class_desc,
    aaslider_class_desc, aaprogress_class_desc, aacycle_class_desc, aafield_class_desc, aascroller_class_desc, aalist_class_desc;

#endif
