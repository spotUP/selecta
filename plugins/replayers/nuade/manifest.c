/* nuade: the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the manifest unit the
 * registry links (I427e4, thoughts/shared/plans/2026-11-10-amiga-loadseg-plugins.md, same split as bbmp_ptk/probe.c).
 * Built with SEL_PLUGIN_MANIFEST this file is the whole registry side of the plugin: the CPU/FPU/memory filter, the
 * format and extension claims and the companion rules run from here, and the players (adapter.c, the shim, every
 * third_party/nuade player) are the LoadSeg module Players/sel_nuade, loaded on the first probe of a song whose name
 * the EXTENSIONS list claims. The manifest's lists come from manifest_lists.h, written by the host tool
 * nuade_manifest_lists from the same registry walk the plugin runs (lists.c); test nuade-manifest-lists guards drift. */
#include "manifest.h"

#include "companion_rules.h"
#include "shim.h"

#include <selecta/replayer.h>

#include <string.h>

static const char* const k_cfg_gain = SEL_NUADE_CFG_GAIN;
static sel_tag g_config[2]; /* SEL_RP_CONFIG list: one entry per option, value = the host config key */

void sel_nuade_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

/* RP_CONTENT_COMPANIONS (I1237): an IFF SMUS score names its instruments (INS1 chunks) but holds none of them. The sonix
 * player reads <name>.instr, and the <name>.ss sample behind it, from an Instruments directory beside the song
 * (Modland keeps them there); a remote song cached alone has neither, so every voice stays silent. Names every
 * instrument twice, all .instr first: a Synthesis instrument has no .ss, a SampledSound one keeps its sample under the
 * same stem. Local open reads the directory itself. */
static int ext_is_smus(const char* name) {
    size_t n = strlen(name);
    return n > 5u && name[n - 5u] == '.' && (name[n - 4u] | 0x20) == 's' && (name[n - 3u] | 0x20) == 'm' &&
           (name[n - 2u] | 0x20) == 'u' && (name[n - 1u] | 0x20) == 's';
}

static unsigned rp_content_companions(const char* name, const uint8_t* data, size_t size, char out[][SEL_RP_COMPANION_NAME_MAX],
                                      unsigned max, const sel_host* host) {
    static const char dir[] = "Instruments/";
    unsigned k = 0, pass;
    size_t pos;
    (void)host;
    if (!ext_is_smus(name)) {
        return 0;
    }
    if (data == NULL) {
        return 1;
    }
    for (pass = 0; pass < 2u; pass++) {
        for (pos = 12u; pos + 8u <= size && k < max;) {
            size_t len = ((size_t)data[pos + 4u] << 24) | ((size_t)data[pos + 5u] << 16) | ((size_t)data[pos + 6u] << 8) | data[pos + 7u];
            size_t adv = 8u + len + (len & 1u);
            if (memcmp(data + pos, "INS1", 4u) == 0 && len > 4u && pos + 8u + len <= size) {
                const uint8_t* nm = data + pos + 12u;
                size_t nl = 0, dl = sizeof dir - 1u;
                const char* ext = pass == 0u ? ".instr" : ".ss";
                size_t el = strlen(ext);
                while (nl < len - 4u && nm[nl] != 0u) {
                    nl++;
                }
                if (nl != 0u && dl + nl + el < SEL_RP_COMPANION_NAME_MAX) {
                    memcpy(out[k], dir, dl);
                    memcpy(out[k] + dl, nm, nl);
                    memcpy(out[k] + dl + nl, ext, el + 1u);
                    k++;
                }
            }
            if (adv < 8u || adv > size - pos) {
                break;
            }
            pos += adv;
        }
    }
    return k;
}

size_t sel_nuade_static_hooks(sel_tag* t, const char* extensions, const char* formats) {
    sel_nuade_set_tag(&g_config[0], SEL_TAG_USER, (uintptr_t)k_cfg_gain);
    sel_nuade_set_tag(&g_config[1], SEL_TAG_END, 0);
    sel_nuade_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "nuade");
    sel_nuade_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    sel_nuade_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_VOICE);
    sel_nuade_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t)extensions);
    sel_nuade_set_tag(&t[4], SEL_RP_FORMATS, (uintptr_t)formats);
    sel_nuade_set_tag(&t[5], SEL_RP_MAX_VOICES, SEL_NUADE_MAX_VOICES);
    /* Needs: 68020 build, floating point in the voice shim (pending the T60 per-player
     * measurement), and a 1.85 MB runtime image (fast RAM counts). */
    sel_nuade_set_tag(&t[6], SEL_RP_MIN_CPU, 68020);
    sel_nuade_set_tag(&t[7], SEL_RP_NEEDS_FLOAT, 1);
    sel_nuade_set_tag(&t[8], SEL_RP_MEMORY, 2097152);
    sel_nuade_set_tag(&t[9], SEL_RP_CONFIG, (uintptr_t)g_config);
    sel_nuade_set_tag(&t[10], SEL_RP_COMPANIONS, (uintptr_t)g_companions);
    sel_nuade_set_tag(&t[11], SEL_RP_CONTENT_COMPANIONS, SEL_TAG_FN(rp_content_companions));
    return 12u;
}

#ifdef SEL_PLUGIN_MANIFEST
#include "manifest_lists.h"
#include "signatures.h"

/* Amiga manifest probe (I1305i2): the players live in the on-demand module, so the scan matches the magic byte
 * table instead of loading it. A signature is a sure answer; a song with no known header that the nuade
 * EXTENSIONS claim is Unsure, so it opens with nuade only when no other replayer is sure. The module's own
 * probe runs at open time. */
static int rp_manifest_probe(const uint8_t* data, size_t size, const char* name) {
    const char* base = name;
    const char* p;
    if (sel_nuade_sig_match(data, size) != NULL) {
        return SEL_PROBE_YES;
    }
    for (p = name; p != NULL && *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    return sel_nuade_sig_name_claimed(SEL_NUADE_MANIFEST_EXTENSIONS, base) ? SEL_PROBE_UNSURE : SEL_PROBE_NO;
}

static sel_tag g_hooks[14];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    size_t n = sel_nuade_static_hooks(g_hooks, SEL_NUADE_MANIFEST_EXTENSIONS, SEL_NUADE_MANIFEST_FORMATS);
    if (host_api < 1u) {
        return NULL;
    }
    sel_nuade_set_tag(&g_hooks[n++], SEL_RP_PROBE, SEL_TAG_FN(rp_manifest_probe));
    sel_nuade_set_tag(&g_hooks[n], SEL_TAG_END, 0);
    sel_nuade_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    sel_nuade_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "nuade");
    sel_nuade_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    sel_nuade_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    sel_nuade_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_nuade");
    sel_nuade_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
