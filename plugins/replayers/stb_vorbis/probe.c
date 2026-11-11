/* stb_vorbis: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the
 * manifest unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the hively
 * pattern). Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the Ogg/Vorbis probe and
 * the CPU/FPU filter run from here, and the decoder (adapter.c plus third_party/stb_vorbis, FPU objects) is the LoadSeg
 * module Players/sel_stb_vorbis, loaded on the first open. Plain C, no float: the manifest needs no FPU flags. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

/* "OggS", then the Vorbis identification packet (type 1, "vorbis") in the first page. Ogg also carries Opus
 * and FLAC, which stb_vorbis cannot decode. */
int vorbis_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i;
    (void)name;
    if (size < 36u || memcmp(data, "OggS", 4) != 0) {
        return SEL_PROBE_NO;
    }
    for (i = 26u; i + 7u <= 64u && i + 7u <= size; i++) {
        if (data[i] == 1u && memcmp(data + i + 1u, "vorbis", 6) == 0) {
            return SEL_PROBE_YES;
        }
    }
    return SEL_PROBE_NO;
}

void vorbis_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t vorbis_static_hooks(sel_tag* t) {
    vorbis_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "stb_vorbis");
    vorbis_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    vorbis_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    vorbis_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "ogg");
    vorbis_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(vorbis_probe));
    vorbis_set_tag(&t[5], SEL_RP_MIN_CPU, 68020);
    vorbis_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* 68881 objects: hidden on a machine without an FPU */
    vorbis_set_tag(&t[7], SEL_RP_STREAMING, 1);
    vorbis_set_tag(&t[8], SEL_RP_NEEDS_FPU, 1); /* I1316: built with -m68881, crashes without an FPU */
    /* I1313: RAM peak 655024 B (module hunk plus allocations) rendering sine.ogg, measured 2026-11-11 by host count_malloc shim plus the Players/ hunk file size (64-bit heap sizes, an upper bound for the 68k
     * build; vamos cannot run FPU code)
     * (tools/player_ram.py). Declared = peak +25%, rounded up to 64 KB; the registry hides the player
     * when this is above the RAM free after Selecta has started. */
    vorbis_set_tag(&t[9], SEL_RP_MEMORY, 851968);
    return 10u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[11];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    vorbis_set_tag(&g_hooks[vorbis_static_hooks(g_hooks)], SEL_TAG_END, 0);
    vorbis_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    vorbis_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "stb_vorbis");
    vorbis_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    vorbis_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    vorbis_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_stb_vorbis");
    vorbis_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
