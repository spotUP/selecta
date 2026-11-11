/* KPlayer: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the bbmp_ptk pattern).
 * Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the magic probe and the CPU filter
 * run from here, and the replayer (adapter.c, modwalk.c and the assembled kpl14.s blob) is the LoadSeg module
 * Players/sel_kplayer, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

#define KP_MAGIC_AT 1080u /* after the 128-byte order table of a 31-sample module */

int kp_has_magic(const uint8_t* data, size_t size) {
    const uint8_t* m;
    if (data == NULL || size < KP_MAGIC_AT + 4u) {
        return 0;
    }
    m = data + KP_MAGIC_AT;
    return memcmp(m, "M.K.", 4) == 0 || memcmp(m, "M!K!", 4) == 0 || memcmp(m, "FLT4", 4) == 0;
}

int kp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return kp_has_magic(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

void kp_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t kp_static_hooks(sel_tag* t) {
    kp_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "kplayer");
    kp_set_tag(&t[1], SEL_RP_VERSION, 0x00210000);
    kp_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_HW);
    kp_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "mod");
    kp_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(kp_probe));
    kp_set_tag(&t[5], SEL_RP_MIN_CPU, 68000);
    kp_set_tag(&t[6], SEL_RP_PRIORITY, 10); /* the native player outranks nuade for M.K. on Amiga */
    return 7u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[8];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    kp_set_tag(&g_hooks[kp_static_hooks(g_hooks)], SEL_TAG_END, 0);
    kp_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    kp_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "kplayer");
    kp_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    kp_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    kp_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_kplayer");
    kp_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
