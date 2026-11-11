/* bbmp ptk: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the
 * manifest unit the registry links (I427e, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md). Built with
 * SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: probing and the FPU filter run from here and
 * the replayer itself (adapter.c, shim.cpp, the ProTrekkr engine) is the LoadSeg module Players/sel_bbmp_ptk, loaded
 * on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

int ptks_probe(const uint8_t* d, size_t n) {
    char v;
    if (n < 64u || (memcmp(d, "PROTREK", 7) != 0 && memcmp(d, "TWNNSNG", 7) != 0)) {
        return 0;
    }
    v = (char)d[7];
    return (v >= '1' && v <= '9') || (v >= 'A' && v <= 'V');
}

static int ext_is(const char* name, char a, char b, char c) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 4u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == a && (name[nl - 2u] | 0x20) == b &&
           (name[nl - 1u] | 0x20) == c;
}

static int bbpk_probe(const uint8_t* data, size_t size, const char* name) {
    if (!ptks_probe(data, size)) {
        return SEL_PROBE_NO;
    }
    return ext_is(name, 'p', 't', 'k') || ext_is(name, 'n', 't', 'k') ? SEL_PROBE_YES : SEL_PROBE_UNSURE;
}

void bbpk_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t bbpk_static_hooks(sel_tag* t) {
    bbpk_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "bbmp_ptk");
    bbpk_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    bbpk_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    bbpk_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "ntk,ptk");
    bbpk_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(bbpk_probe));
    bbpk_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    bbpk_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* float mixer: hidden on a machine without an FPU */
    /* I1313: RAM peak 386408 B (module hunk plus allocations) rendering wonderland.ptk, measured 2026-11-11 by vamos -l mem_alloc:info up to where the vamos run stopped (UNVERIFIED: the song did not
     * play under vamos; re-measure on a rig)
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    bbpk_set_tag(&t[7], SEL_RP_MEMORY, 524288);
    return 8u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[9];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    bbpk_set_tag(&g_hooks[bbpk_static_hooks(g_hooks)], SEL_TAG_END, 0);
    bbpk_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    bbpk_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_ptk");
    bbpk_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    bbpk_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    bbpk_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_bbmp_ptk");
    bbpk_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
