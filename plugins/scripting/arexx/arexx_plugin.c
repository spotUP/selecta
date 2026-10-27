/* Registry entry of the ARexx scripting plugin (kind 9). The shell opens the port through arexx.h; the plugin has
 * no hooks the registry calls. */
#include <selecta/plugin.h>
#include <stddef.h>

static sel_tag g_desc[4];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    g_desc[0].tag = SEL_PLUGIN_KIND;
    g_desc[0].value = SEL_PLUGIN_KIND_SCRIPTING;
    g_desc[1].tag = SEL_PLUGIN_ID;
    g_desc[1].value = (uintptr_t) "arexx";
    g_desc[2].tag = SEL_PLUGIN_VERSION;
    g_desc[2].value = 1;
    g_desc[3].tag = SEL_TAG_END;
    g_desc[3].value = 0;
    return g_desc;
}
