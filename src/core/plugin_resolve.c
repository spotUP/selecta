/* On-demand plugin modules (I427e, plugin-loader-sdk D8): the resolver slot the engine and the library call
 * before using a replayer's hooks. Kept in selecta_core because neither may link the registry; the registry's
 * Amiga loader (src/engine/loader_seg.c) installs the resolver at registry init. */
#include "../engine/claims.h"

#include <selecta/machine.h>
#include <selecta/registry.h>
#include <selecta/replayer.h>

#include <stddef.h>

static sel_plugin_resolve_fn g_resolver;
static sel_plugin_resolve_fn g_manifest_of;

void sel_plugin_set_resolver(sel_plugin_resolve_fn fn, sel_plugin_resolve_fn manifest_of) {
    g_resolver = fn;
    g_manifest_of = manifest_of;
}

const sel_tag* sel_plugin_resolve(const sel_tag* hooks) {
    return g_resolver != NULL && hooks != NULL ? g_resolver(hooks) : hooks;
}

/* I1314b: unloading idle modules after a scan. The loader installs the mark/unload pair; the engine holds the
 * real hooks of every open song here, so the unload skips a module current playback runs. One small table: at
 * most a few slots play at once (current, next, crossfade). */
enum { kHeldMax = 16 };
static struct {
    const sel_tag* hooks;
    uint32_t n;
} g_held[kHeldMax];
static sel_plugin_mark_fn g_mark;
static sel_plugin_unload_fn g_unload;

void sel_plugin_set_unloader(sel_plugin_mark_fn mark, sel_plugin_unload_fn unload) {
    g_mark = mark;
    g_unload = unload;
}

uint32_t sel_plugin_load_mark(void) {
    return g_mark != NULL ? g_mark() : 0u;
}

uint32_t sel_plugin_unload_since(uint32_t mark) {
    return g_unload != NULL ? g_unload(mark) : 0u;
}

void sel_plugin_hold(const sel_tag* hooks, int hold) {
    uint32_t i, free_i = kHeldMax;
    if (hooks == NULL) {
        return;
    }
    for (i = 0; i < kHeldMax; i++) {
        if (g_held[i].n != 0u && g_held[i].hooks == hooks) {
            if (hold) {
                g_held[i].n++;
            } else {
                g_held[i].n--;
            }
            return;
        }
        if (g_held[i].n == 0u && free_i == kHeldMax) {
            free_i = i;
        }
    }
    if (hold && free_i < kHeldMax) {
        g_held[free_i].hooks = hooks;
        g_held[free_i].n = 1u;
    }
}

int sel_plugin_held(const sel_tag* hooks) {
    uint32_t i;
    for (i = 0; i < kHeldMax; i++) {
        if (g_held[i].n != 0u && g_held[i].hooks == hooks) {
            return 1;
        }
    }
    return 0;
}

int sel_replayer_outranks(const sel_tag* cand, const sel_tag* best, int has_fpu) {
    int cand_slow, best_slow;
    intptr_t cand_pri, best_pri;
    if (best == NULL) {
        return 1;
    }
    /* Without an FPU a SEL_RP_NEEDS_FLOAT player runs on soft-float: it stays usable but any player without the tag
     * goes first, whatever the priorities (Johan 2026-11-11, I1316). */
    cand_slow = !has_fpu && sel_tag_find(cand, SEL_RP_NEEDS_FLOAT, 0) != 0;
    best_slow = !has_fpu && sel_tag_find(best, SEL_RP_NEEDS_FLOAT, 0) != 0;
    if (cand_slow != best_slow) {
        return best_slow;
    }
    cand_pri = (intptr_t)sel_tag_find(cand, SEL_RP_PRIORITY, 0);
    best_pri = (intptr_t)sel_tag_find(best, SEL_RP_PRIORITY, 0);
    return cand_pri > best_pri; /* ties: the earlier one stays */
}

int sel_machine_has_fpu(const sel_tag* machine) {
    return machine == NULL || sel_tag_find(machine, SEL_MACHINE_FPU, 0) != 0;
}

const sel_tag* sel_plugin_manifest(const sel_tag* hooks) {
    return g_manifest_of != NULL && hooks != NULL ? g_manifest_of(hooks) : hooks;
}

const char* sel_plugin_owner_id(uint32_t (*count)(void), const sel_tag* (*get)(uint32_t), const sel_tag* hooks) {
    const sel_tag* key = sel_plugin_manifest(hooks); /* a loaded module's hooks are listed under their manifest */
    uint32_t i, n = count != NULL && get != NULL ? count() : 0;

    for (i = 0; i < n; i++) {
        const sel_tag* desc = get(i);

        if ((const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) == key) {
            return (const char*)sel_tag_find(desc, SEL_PLUGIN_ID, (uintptr_t) "?");
        }
    }
    return "?";
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
