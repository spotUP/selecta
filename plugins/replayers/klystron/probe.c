/* Klystrack: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I1305m, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the bbmp_ptk pattern, as
 * hively I427e4). Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the signature probe
 * and the FPU filter run from here, and the replayer (adapter.c plus the klystron engine, which uses float) is the LoadSeg
 * module Players/sel_klystron, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

int kt_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size >= KT_SIG_LEN + 1u && memcmp(data, KT_SIG, KT_SIG_LEN) == 0) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

void kt_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t kt_static_hooks(sel_tag* t) {
    kt_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "klystron");
    kt_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    kt_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    kt_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "kt");
    kt_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(kt_probe));
    kt_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    kt_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* the engine uses float: hidden on a machine without an FPU */
    return 7u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[8];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    kt_set_tag(&g_hooks[kt_static_hooks(g_hooks)], SEL_TAG_END, 0);
    kt_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    kt_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "klystron");
    kt_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    kt_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    kt_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_klystron");
    kt_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
