/* pxtone: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the hively/bbmp_ptk
 * pattern). Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the header probe and the
 * FPU filter run from here, and the replayer (adapter.c plus shim.cpp, the C++ engine third_party/pxtone and libstdc++) is the
 * LoadSeg module Players/sel_pxtone, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

int px_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (data == NULL || size < 16u) {
        return SEL_PROBE_NO;
    }
    return memcmp(data, "PTCOLLAGE-", 10) == 0 || memcmp(data, "PTTUNE--", 8) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

void px_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t px_static_hooks(sel_tag* t) {
    px_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "pxtone");
    px_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    px_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    px_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "ptcop,pttune");
    px_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(px_probe));
    px_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    px_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* the song data path uses float and libm: hidden on a machine without an FPU */
    return 7u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[8];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    px_set_tag(&g_hooks[px_static_hooks(g_hooks)], SEL_TAG_END, 0);
    px_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    px_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "pxtone");
    px_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    px_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    px_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_pxtone");
    px_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
