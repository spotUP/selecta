/* Hively: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the bbmp_ptk pattern).
 * Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the header probe and the FPU filter
 * run from here, and the replayer (adapter.c plus third_party/hively with its 411 KB `waves` table) is the LoadSeg
 * module Players/sel_hively, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

int hvl_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size < HVL_MIN_SIZE) {
        return SEL_PROBE_NO;
    }
    if (data[0] == 'T' && data[1] == 'H' && data[2] == 'X' && data[3] < 3u) {
        return SEL_PROBE_YES;
    }
    if (data[0] == 'H' && data[1] == 'V' && data[2] == 'L' && data[3] < 2u) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

void hvl_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t hvl_static_hooks(sel_tag* t) {
    hvl_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "hively");
    hvl_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    hvl_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    hvl_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "ahx,thx,hvl");
    hvl_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(hvl_probe));
    hvl_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    hvl_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* soft-float replayer: hidden on a machine without an FPU */
    /* I1313: RAM peak 1690692 B (module hunk plus allocations) rendering hively_raining_stars.ahx, measured 2026-11-11 by vamos -l mem_alloc:info, render peak minus a no-player baseline
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    hvl_set_tag(&t[7], SEL_RP_MEMORY, 2162688);
    return 8u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[9];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    hvl_set_tag(&g_hooks[hvl_static_hooks(g_hooks)], SEL_TAG_END, 0);
    hvl_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    hvl_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "hively");
    hvl_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    hvl_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    hvl_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_hively");
    hvl_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
