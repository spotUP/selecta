/* dr_flac: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the hively pattern).
 * Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the "fLaC" marker probe runs from
 * here, and the decoder (adapter.c plus third_party/dr_flac) is the LoadSeg module Players/sel_dr_flac, loaded on the
 * first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

int flac_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 8u && memcmp(data, "fLaC", 4) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

void flac_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t flac_static_hooks(sel_tag* t) {
    flac_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "dr_flac");
    flac_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    flac_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    flac_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "flac");
    flac_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(flac_probe));
    /* I1313: RAM peak 143912 B (module hunk plus allocations) rendering cover_tagged.flac, measured 2026-11-11 by vamos -l mem_alloc:info, render peak minus a no-player baseline
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    flac_set_tag(&t[5], SEL_RP_MEMORY, 196608);
    return 6u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[7];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    flac_set_tag(&g_hooks[flac_static_hooks(g_hooks)], SEL_TAG_END, 0);
    flac_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    flac_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "dr_flac");
    flac_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    flac_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    flac_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_dr_flac");
    flac_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
