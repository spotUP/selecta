/* bbmp ay: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest
 * unit the registry links (I1223c, as bbmp_ptk I427e). Built with SEL_PLUGIN_MANIFEST this file is the whole registry side;
 * the replayer (adapter.c, shim.cpp, the engines) is the LoadSeg module Players/sel_bbmp_ay, loaded on the first open. */
#include "probe.h"

#include <selecta/replayer.h>

#include <string.h>

static int ext_is(const char* name, const char* ext) {
    const char* dot;
    size_t i;
    if (name == NULL || (dot = strrchr(name, '.')) == NULL || strlen(dot + 1) != strlen(ext)) {
        return 0;
    }
    for (i = 0; ext[i] != '\0'; i++) {
        if ((dot[1 + i] | 0x20) != ext[i]) {
            return 0;
        }
    }
    return 1;
}

/* The one decider of which engine owns a file: the AY magic first, then the extension of the formats with no magic. */
int bbay_kind_of(const uint8_t* data, size_t size, const char* name) {
    if (size >= 8u && memcmp(data, "ZXAYAMAD", 8) == 0) {
        return KIND_FXM;
    }
    if (size >= 8u && memcmp(data, "ZXAYSTRC", 8) == 0) {
        return KIND_STRC;
    }
    if (size >= 12u && memcmp(data, "BBSONG\0" "0001\0", 12) == 0) {
        return KIND_BEEPOLA;
    }
    if (ext_is(name, "fls")) {
        return KIND_FLS;
    }
    if (ext_is(name, "fxm")) {
        return KIND_FXM;
    }
    return KIND_NONE;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size < 64u) {
        return SEL_PROBE_NO;
    }
    return bbay_kind_of(data, size, name) != KIND_NONE ? SEL_PROBE_YES : SEL_PROBE_NO;
}

void bbay_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t bbay_static_hooks(sel_tag* t) {
    bbay_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "bbmp_ay");
    bbay_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    bbay_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    bbay_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "fls,fxm,amad,strc,ay,bbsong");
    bbay_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    /* above gme (0), which also claims .ay; the probe answers YES only for the two AY types and the extensions */
    bbay_set_tag(&t[5], SEL_RP_PRIORITY, 10);
    /* RAM: UNMEASURED (I1223c). The single-thread Beepola path holds up to 12 MB of rendered PCM plus the engine;
     * 16 MB declared until a rig run measures it (tools/player_ram.py). */
    bbay_set_tag(&t[6], SEL_RP_MEMORY, 16u * 1024u * 1024u);
    return 7u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[8];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    bbay_set_tag(&g_hooks[bbay_static_hooks(g_hooks)], SEL_TAG_END, 0);
    bbay_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    bbay_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_ay");
    bbay_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    bbay_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    bbay_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_bbmp_ay");
    bbay_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
