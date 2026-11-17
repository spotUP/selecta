/* Arkos Tracker 3: the probe and the static hook facts, shared by the full plugin (adapter.c) and, on the Amiga, by the
 * manifest unit the registry links (I427e4, the dr_flac pattern). The player (adapter.c, at3_player.c, at3_song.c and
 * Ayumi) is the LoadSeg module Players/sel_arkos, loaded on the first open. */
#include "probe.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

static int has_ext(const char* name, const char* ext) {
    size_t n = name ? strlen(name) : 0u, e = strlen(ext), i;
    if (n <= e || name[n - e - 1u] != '.') {
        return 0;
    }
    for (i = 0; i < e; i++) {
        char c = name[n - e + i];
        if ((c >= 'A' && c <= 'Z' ? c + 32 : c) != ext[i]) {
            return 0;
        }
    }
    return 1;
}

int arkos_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i, n = size < 512u ? size : 512u;
    if (size < 8u) {
        return SEL_PROBE_NO;
    }
    /* AT3 saves a zip holding one XML song; a zip only counts under the .aks name, the importer checks the rest. */
    if (memcmp(data, "PK\3\4", 4) == 0) {
        return has_ext(name, "aks") ? SEL_PROBE_UNSURE : SEL_PROBE_NO;
    }
    for (i = 0; i + 5u <= n; i++) {
        if (memcmp(data + i, "<song", 5) == 0) {
            return has_ext(name, "aks") ? SEL_PROBE_YES : SEL_PROBE_UNSURE;
        }
    }
    return SEL_PROBE_NO;
}

void arkos_set_tag(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

size_t arkos_static_hooks(sel_tag* t) {
    arkos_set_tag(&t[0], SEL_RP_NAME, (uintptr_t) "arkos");
    arkos_set_tag(&t[1], SEL_RP_VERSION, 0x00010000);
    arkos_set_tag(&t[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    arkos_set_tag(&t[3], SEL_RP_EXTENSIONS, (uintptr_t) "aks");
    arkos_set_tag(&t[4], SEL_RP_PROBE, SEL_TAG_FN(arkos_probe));
    /* shortcut: not measured yet (player state ~30 KB plus the song model); measure with tools/player_ram.py on Amiga. */
    arkos_set_tag(&t[5], SEL_RP_MEMORY, 524288);
    arkos_set_tag(&t[6], SEL_RP_NEEDS_FLOAT, 1); /* Ayumi runs in double */
    return 7u;
}

#ifdef SEL_PLUGIN_MANIFEST
static sel_tag g_hooks[8];
static sel_tag g_desc[6];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    arkos_set_tag(&g_hooks[arkos_static_hooks(g_hooks)], SEL_TAG_END, 0);
    arkos_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    arkos_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "arkos");
    arkos_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    arkos_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    arkos_set_tag(&g_desc[4], SEL_PLUGIN_MODULE, (uintptr_t) "sel_arkos");
    arkos_set_tag(&g_desc[5], SEL_TAG_END, 0);
    return g_desc;
}
#endif
