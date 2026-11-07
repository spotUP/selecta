/* Spec to tag list (I685f, design 2026-11-06-mui-mcc-package.md section 8.3). Selecta's wrappers (mui_aawrap.c) build the tag list
 * of a UP- class from the spec struct with these functions, then call the class constructor and add the ReturnID notify. Pure C
 * and host-testable: nothing here calls MUI. Every builder writes the tags followed by TAG_DONE into out[cap] and returns the
 * number of items written including TAG_DONE, or 0 when cap is too small. The spec's id is never a tag. Strings and arrays are
 * referenced, not copied: the spec's data must outlive the object, as before. */
#ifndef MUI_AATAGMAP_H
#define MUI_AATAGMAP_H

#include <stddef.h>

#include "mui_aabtn_class.h"
#include "mui_aacycle_class.h"
#include "mui_aafield_class.h"
#include "mui_aalist_class.h"
#include "mui_aapanel_class.h"
#include "mui_aaslider_class.h"
#include "mui_aatabs_class.h"
#include "mui_aatext_class.h"

#define MUI_AATAGMAP_CELL 256 /* bytes of one list cell as Latin-1 (I1139) */
#define MUI_AATAGMAP_MAX 24 /* items every builder needs at most, TAG_DONE included */

int mui_aabtn_tags(const mui_aabtn_spec* sp, struct TagItem* out, size_t cap);
int mui_aatext_tags(const mui_aatext_spec* sp, struct TagItem* out, size_t cap);
int mui_aaslider_tags(const mui_aaslider_spec* sp, struct TagItem* out, size_t cap);
int mui_aacycle_tags(const mui_aacycle_spec* sp, struct TagItem* out, size_t cap);
int mui_aatabs_tags(const mui_aatabs_spec* sp, struct TagItem* out, size_t cap);
int mui_aafield_tags(const mui_aafield_spec* sp, struct TagItem* out, size_t cap);
int mui_aapanel_tags(const mui_aapanel_spec* sp, struct TagItem* out, size_t cap);

/* The list's hooks: the wrapper owns this storage (it must outlive the list, never the stack). The builder fills both hooks'
 * h_SubEntry and h_Data (the spec's ctx); the cell hook wins when the spec has both; the Amiga wrapper sets h_Entry to HookEntry. A hook not needed has h_SubEntry NULL and
 * is not in the tag list. */
typedef struct mui_aalist_row_hook {
    struct Hook hook; /* first: the hook body finds its holder from the Hook pointer */
    mui_aalist_row_fn fn;
    char l1[MUI_AATAGMAP_CELL]; /* I1139: the owner's UTF-8 row text as Latin-1, valid until the next call of this hook */
} mui_aalist_row_hook;
typedef struct mui_aalist_cell_hook {
    struct Hook hook;
    mui_aalist_cell_fn fn;
    char l1[MUI_AATAGMAP_CELL]; /* I1139: as in the row hook */
} mui_aalist_cell_hook;
typedef struct mui_aalist_hooks {
    mui_aalist_row_hook row;
    mui_aalist_cell_hook cell;
} mui_aalist_hooks;
int mui_aalist_tags(const mui_aalist_spec* sp, mui_aalist_hooks* hooks, struct TagItem* out, size_t cap);

#endif
