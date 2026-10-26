/* Builds a source plugin's descriptor tag list around its sel_source (I517 phase 3). The library finds the source with
 * sel_tag_find(hooks, SEL_SRC_DESCRIPTOR, 0). One plugin per translation unit includes this: the lists are static. */
#ifndef SELECTA_SHARED_SOURCE_DESCRIPTOR_H
#define SELECTA_SHARED_SOURCE_DESCRIPTOR_H

#include <selecta/plugin.h>
#include <selecta/source.h>

static sel_tag sp_hooks[2];
static sel_tag sp_desc[5];

static inline const sel_tag* sel_source_plugin_descriptor(const char* id, const sel_source* src) {
    sp_hooks[0].tag = SEL_SRC_DESCRIPTOR;
    sp_hooks[0].value = (uintptr_t)src;
    sp_hooks[1].tag = SEL_TAG_END;
    sp_hooks[1].value = 0;
    sp_desc[0].tag = SEL_PLUGIN_KIND;
    sp_desc[0].value = SEL_PLUGIN_KIND_SOURCE;
    sp_desc[1].tag = SEL_PLUGIN_ID;
    sp_desc[1].value = (uintptr_t)id;
    sp_desc[2].tag = SEL_PLUGIN_VERSION;
    sp_desc[2].value = 1;
    sp_desc[3].tag = SEL_PLUGIN_TAGS;
    sp_desc[3].value = (uintptr_t)sp_hooks;
    sp_desc[4].tag = SEL_TAG_END;
    sp_desc[4].value = 0;
    return sp_desc;
}

#endif
