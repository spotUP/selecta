/* Tag-list ABI (design doc section 10). A tag list is an array of
 * sel_tag terminated by SEL_TAG_END. Plugins declare only what they
 * implement; the host skips unknown tags and asks for a capability by
 * tag, never by struct size or offset. */
#ifndef SELECTA_TAGS_H
#define SELECTA_TAGS_H

#include <selecta/api.h>

SEL_EXTERN_C_BEGIN

typedef struct sel_tag {
    uint32_t tag;
    uintptr_t value;
} sel_tag;

#define SEL_TAG_END 0u  /* end of list */
#define SEL_TAG_MORE 1u /* value: const sel_tag* to continue in */
#define SEL_TAG_SKIP 2u /* skip this entry and the next `value` entries */
#define SEL_TAG_USER 0x80000000u /* plugin-private tag, never in tags.def */

/* Every public tag, generated from tags.def: SEL_OUT_NAME, SEL_META_TITLE, ... */
enum sel_tag_id {
#define SEL_TAG(ns, name, value, type) SEL_##ns##_##name = (value),
#include <selecta/tags.def>
#undef SEL_TAG
    SEL_TAG_ID_END_MARKER_ = 0x7FFFFFFF /* not a tag; keeps the enum 32-bit wide */
};

typedef enum sel_tag_type { SEL_TT_INT = 0, SEL_TT_STR, SEL_TT_PTR, SEL_TT_LIST } sel_tag_type;

typedef struct sel_tag_info {
    uint32_t tag;
    const char* name; /* "OUT_NAME" */
    sel_tag_type type;
} sel_tag_info;

/* Walk a list, following MORE and SKIP. *iter starts at the list head;
 * each call returns the next data tag and advances, NULL at the end. */
const sel_tag* sel_tag_next(const sel_tag** iter);

/* First value for `tag` in the list, `def` when absent. */
uintptr_t sel_tag_find(const sel_tag* list, uint32_t tag, uintptr_t def);

/* Number of data tags in the list (MORE followed, SKIP honoured). */
uint32_t sel_tag_count(const sel_tag* list);

/* Reflection over tags.def. Unknown tag: NULL / 0. */
const char* sel_tag_name(uint32_t tag);
int sel_tag_type_of(uint32_t tag, sel_tag_type* out);
int sel_tag_by_name(const char* name, uint32_t* out);
const sel_tag_info* sel_tag_table(uint32_t* count);

SEL_EXTERN_C_END

#endif /* SELECTA_TAGS_H */
