/* Plugin entry point (design doc section 10). A plugin exports one
 * function that returns its descriptor: a tag list with PLUGIN_KIND,
 * PLUGIN_ID, PLUGIN_VERSION and PLUGIN_TAGS (the kind-specific hooks).
 * Static builds rename the entry per plugin through SEL_PLUGIN_ENTRY so
 * many plugins link into one binary. */
#ifndef SELECTA_PLUGIN_H
#define SELECTA_PLUGIN_H

#include <selecta/host.h>

SEL_EXTERN_C_BEGIN

enum sel_plugin_kind {
    SEL_PLUGIN_KIND_REPLAYER = 1,
    SEL_PLUGIN_KIND_OUTPUT = 2,
    SEL_PLUGIN_KIND_DSP = 3,
    SEL_PLUGIN_KIND_VIZ = 4,
    SEL_PLUGIN_KIND_UI = 5,
    SEL_PLUGIN_KIND_CONVERTER = 6,
    SEL_PLUGIN_KIND_SOURCE = 7,
    SEL_PLUGIN_KIND_INTEGRATION = 8,
    SEL_PLUGIN_KIND_SCRIPTING = 9
};

typedef const sel_tag* (*sel_plugin_entry_fn)(uint32_t host_api);

/* Stores a function pointer in a tag value. */
#define SEL_TAG_FN(fn) ((uintptr_t)(fn))

#ifndef SEL_PLUGIN_ENTRY
#define SEL_PLUGIN_ENTRY sel_plugin_entry
#endif

/* Returns the descriptor, or NULL when host_api is not supported. */
const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api);

SEL_EXTERN_C_END

#endif /* SELECTA_PLUGIN_H */
