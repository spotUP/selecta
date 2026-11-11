/* Organya: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the bbmp_ptk pattern).
 * Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the header probe and the FPU filter
 * run from here, and the replayer (adapter.c plus third_party/organya) is the LoadSeg module Players/sel_organya, loaded
 * on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

static int ext_is_org(const char* name) {
    const char* dot;
    if (name == NULL || (dot = strrchr(name, '.')) == NULL || strlen(dot) != 4u) {
        return 0;
    }
    return (dot[1] | 0x20) == 'o' && (dot[2] | 0x20) == 'r' && (dot[3] | 0x20) == 'g';
}

int org_probe(const uint8_t* data, size_t size, const char* name) {
    if (size >= 6u && memcmp(data, "Org-0", 5u) == 0 && data[5] >= '1' && data[5] <= '3') {
        return SEL_PROBE_YES;
    }
    return (ext_is_org(name) && size >= 6u && memcmp(data, "Org-", 4u) == 0) ? SEL_PROBE_UNSURE : SEL_PROBE_NO;
}

void org_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t org_static_hooks(sel_tag* t) {
    org_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "organya");
    org_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    org_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    org_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "org");
    org_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(org_probe));
    org_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    /* Soft-float replayer: on a machine without an FPU it ranks below players without the tag (I1316). */
    org_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1);
    return 7u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[8];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    org_set_tag(&g_hooks[org_static_hooks(g_hooks)], SEL_TAG_END, 0);
    org_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    org_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "organya");
    org_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    org_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    org_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_organya");
    org_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
