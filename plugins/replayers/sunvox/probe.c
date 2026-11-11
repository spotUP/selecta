/* SunVox: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the hively/bbmp_ptk
 * pattern). Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the header probe and the
 * FPU filter run from here, and the replayer (adapter.c plus the C++ engine third_party/sunvox_bsd and libstdc++) is the
 * LoadSeg module Players/sel_sunvox, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

int sv_probe(const uint8_t* data, size_t size, const char* name) {
    uint32_t v;
    (void)name;
    if (data == NULL || size < 20u || memcmp(data, "SVOX", 4) != 0 || memcmp(data + 8, "VERS", 4) != 0) {
        return SEL_PROBE_NO;
    }
    v = (uint32_t)data[16] | ((uint32_t)data[17] << 8) | ((uint32_t)data[18] << 16) | ((uint32_t)data[19] << 24);
    return v > SV_MAX_VERSION ? SEL_PROBE_NO : SEL_PROBE_YES;
}

void sv_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t sv_static_hooks(sel_tag* t) {
    sv_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "sunvox");
    sv_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    sv_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    sv_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "sunvox");
    sv_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(sv_probe));
    sv_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    sv_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* the engine mixes floats: hidden on a machine without an FPU */
    /* I1313: RAM peak 1743516 B (module hunk plus allocations) rendering sunvox_tune.sunvox, measured 2026-11-11 by vamos -l mem_alloc:info, render peak minus a no-player baseline
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    sv_set_tag(&t[7], SEL_RP_MEMORY, 2228224);
    return 8u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[9];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    sv_set_tag(&g_hooks[sv_static_hooks(g_hooks)], SEL_TAG_END, 0);
    sv_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    sv_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sunvox");
    sv_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    sv_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    sv_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_sunvox");
    sv_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
