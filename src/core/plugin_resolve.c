/* On-demand plugin modules (I427e, plugin-loader-sdk D8): the resolver slot the engine and the library call
 * before using a replayer's hooks. Kept in selecta_core because neither may link the registry; the registry's
 * Amiga loader (src/engine/loader_seg.c) installs the resolver at registry init. */
#include "../engine/claims.h"

#include <selecta/registry.h>
#include <selecta/replayer.h>

#include <stddef.h>

static sel_plugin_resolve_fn g_resolver;

void sel_plugin_set_resolver(sel_plugin_resolve_fn fn) {
    g_resolver = fn;
}

const sel_tag* sel_plugin_resolve(const sel_tag* hooks) {
    return g_resolver != NULL && hooks != NULL ? g_resolver(hooks) : hooks;
}

int sel_plugin_probe(const sel_tag** hooks, const uint8_t* data, size_t size, const char* path) {
    sel_rp_probe_fn probe = *hooks != NULL ? (sel_rp_probe_fn)sel_tag_find(*hooks, SEL_RP_PROBE, 0) : NULL;
    if (probe == NULL && g_resolver != NULL && path != NULL) {
        /* A manifest whose probe lives in its module (nuade: the probe asks every player, I427e4) loads the module
         * only for a song whose name its EXTENSIONS claim, so a scan of other formats never pays for it. */
        const char* name = path;
        const char* p;
        for (p = path; *p != '\0'; p++) {
            if (*p == '/' || *p == ':') {
                name = p + 1;
            }
        }
        if (sel_hooks_claim_name(*hooks, name)) {
            const sel_tag* real = sel_plugin_resolve(*hooks);
            probe = real != NULL ? (sel_rp_probe_fn)sel_tag_find(real, SEL_RP_PROBE, 0) : NULL;
            if (probe != NULL) {
                *hooks = real;
            }
        }
    }
    return probe != NULL ? probe(data, size, path) : SEL_PROBE_NO;
}
