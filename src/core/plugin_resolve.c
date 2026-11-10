/* On-demand plugin modules (I427e, plugin-loader-sdk D8): the resolver slot the engine and the library call
 * before using a replayer's hooks. Kept in selecta_core because neither may link the registry; the registry's
 * Amiga loader (src/engine/loader_seg.c) installs the resolver at registry init. */
#include <selecta/registry.h>

#include <stddef.h>

static sel_plugin_resolve_fn g_resolver;

void sel_plugin_set_resolver(sel_plugin_resolve_fn fn) {
    g_resolver = fn;
}

const sel_tag* sel_plugin_resolve(const sel_tag* hooks) {
    return g_resolver != NULL && hooks != NULL ? g_resolver(hooks) : hooks;
}
