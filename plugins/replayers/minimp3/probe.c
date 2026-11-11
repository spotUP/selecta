/* minimp3: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the
 * manifest unit the registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md; the hively
 * pattern). Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the MPEG header probe
 * and the CPU/FPU filter run from here, and the decoder (adapter.c plus third_party/minimp3, FPU objects) is the
 * LoadSeg module Players/sel_minimp3, loaded on the first open. Integer code only: the manifest needs no FPU flags. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

size_t mp3_id3v2_len(const uint8_t* d, size_t n) {
    if (n >= 10u && d[0] == 'I' && d[1] == 'D' && d[2] == '3' && d[3] < 0xFFu && d[4] < 0xFFu &&
        ((d[6] | d[7] | d[8] | d[9]) & 0x80u) == 0u) {
        return 10u + (((size_t)d[6] << 21) | ((size_t)d[7] << 14) | ((size_t)d[8] << 7) | d[9]) +
               ((d[5] & 0x10u) != 0u ? 10u : 0u);
    }
    return 0;
}

/* Length of the Layer III frame whose header is at `h`, 0 if it is not a fixed-bitrate Layer III header. */
static size_t frame_len(const uint8_t* h) {
    static const uint8_t br1[16] = {0, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 0}; /* x8 kbit/s */
    static const uint8_t br2[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 0};   /* x8 kbit/s */
    static const uint16_t sr[3] = {44100, 48000, 32000};
    unsigned ver = (h[1] >> 3) & 3u, idx = (unsigned)(h[2] >> 4), rate = (h[2] >> 2) & 3u;
    unsigned bitrate, hz;
    if (h[0] != 0xFFu || (h[1] & 0xE0u) != 0xE0u || ver == 1u || (h[1] & 6u) != 2u || idx == 0u || idx == 15u ||
        rate == 3u) {
        return 0;
    }
    bitrate = (ver == 3u ? br1[idx] : br2[idx]) * 8000u;
    hz = sr[rate] >> (ver == 3u ? 0 : (ver == 2u ? 1 : 2));
    return (size_t)((ver == 3u ? 144u : 72u) * bitrate / hz) + ((h[2] >> 1) & 1u);
}

/* Two consecutive valid headers within the first 4 KB after the tag. */
int mp3_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i = mp3_id3v2_len(data, size), limit = i + 4096u;
    (void)name;
    for (; i + 4u <= size && i < limit; i++) {
        size_t n = frame_len(data + i);
        if (n != 0u && i + n + 4u <= size && frame_len(data + i + n) != 0u) {
            return SEL_PROBE_YES;
        }
    }
    return SEL_PROBE_NO;
}

void mp3_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t mp3_static_hooks(sel_tag* t) {
    mp3_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "minimp3");
    mp3_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    mp3_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    mp3_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "mp3,mp2");
    mp3_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(mp3_probe));
    mp3_set_tag(&t[5], SEL_RP_MIN_CPU, 68020);
    mp3_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* 68881 objects: hidden on a machine without an FPU */
    mp3_set_tag(&t[7], SEL_RP_STREAMING, 1);
    return 8u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[9];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    mp3_set_tag(&g_hooks[mp3_static_hooks(g_hooks)], SEL_TAG_END, 0);
    mp3_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    mp3_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "minimp3");
    mp3_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    mp3_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    mp3_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_minimp3");
    mp3_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
