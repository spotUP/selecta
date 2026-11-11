/* playsid: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the bbmp_ptk pattern).
 * Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the header probe, the library
 * presence check and the CPU filter run from here, and the adapter (adapter.c, playsid_map.c and the LVO layer
 * playsid_amiga.c) is the LoadSeg module Players/sel_playsid, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

#if defined(SEL_PLUGIN_MANIFEST) && defined(__m68k__)
#include <exec/libraries.h>
#include <proto/exec.h>
#else
#include "playsid_api.h"
#endif

#define PS_HEADER_MIN 0x76u

uint32_t sel_playsid_probes;

int psid_has_magic(const uint8_t* data, size_t size) {
    return data != NULL && size >= PS_HEADER_MIN && (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0);
}

/* 1 when playsid.library opens. The manifest asks exec directly: the LVO layer (ps_ops) lives in the module, and the
 * probe must not load the module just to learn that the library is missing. The full plugin goes through ps_ops so
 * the host test's stub answers. */
static int library_present(void) {
#if defined(SEL_PLUGIN_MANIFEST) && defined(__m68k__)
    struct Library* base = OpenLibrary((CONST_STRPTR) "playsid.library", 0);
    if (base == NULL) {
        return 0;
    }
    CloseLibrary(base);
    return 1;
#else
    const ps_ops* ops = ps_ops_get();
    if (!ops->open()) {
        return 0;
    }
    ops->close();
    return 1;
#endif
}

int psid_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    sel_playsid_probes++;
    if (!psid_has_magic(data, size)) {
        return SEL_PROBE_NO;
    }
    return library_present() ? SEL_PROBE_YES : SEL_PROBE_NO; /* library not installed: the next SID owner takes the file */
}

void psid_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t psid_static_hooks(sel_tag* t) {
    psid_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "playsid");
    psid_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    psid_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_HW);
    psid_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "sid,psid");
    psid_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(psid_probe));
    psid_set_tag(&t[5], SEL_RP_MIN_CPU, 68020);
    psid_set_tag(&t[6], SEL_RP_PRIORITY, 20); /* above nuade and libsidplayfp (priority 0) */
    /* I1313: RAM peak 2027616 B (module hunk plus allocations) rendering sid_test.sid, measured 2026-11-11 by vamos -l mem_alloc:info up to
     * where the vamos run stopped (UNVERIFIED: the song did not play under vamos; re-measure on a rig)
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    psid_set_tag(&t[7], SEL_RP_MEMORY, 2555904);
    return 8u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[9];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    psid_set_tag(&g_hooks[psid_static_hooks(g_hooks)], SEL_TAG_END, 0);
    psid_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    psid_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "playsid");
    psid_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    psid_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    psid_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_playsid");
    psid_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
