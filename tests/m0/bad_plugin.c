/* T5 negative path: a descriptor whose hook list carries a tag with no
 * tags.def row. Built only with -DSEL_TEST_BAD_PLUGIN=ON; the lint must
 * count it and selecta-cli plugins --lint must fail. */
#include <selecta/plugin.h>

static sel_tag g_hooks[2];
static sel_tag g_desc[5];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    (void)host_api;
    g_hooks[0].tag = 0x2FFFu; /* deliberately unknown */
    g_hooks[0].value = 0;
    g_hooks[1].tag = SEL_TAG_END;
    g_hooks[1].value = 0;
    g_desc[0].tag = SEL_PLUGIN_KIND;
    g_desc[0].value = SEL_PLUGIN_KIND_OUTPUT;
    g_desc[1].tag = SEL_PLUGIN_ID;
    g_desc[1].value = (uintptr_t) "bad";
    g_desc[2].tag = SEL_PLUGIN_VERSION;
    g_desc[2].value = 1;
    g_desc[3].tag = SEL_PLUGIN_TAGS;
    g_desc[3].value = (uintptr_t)g_hooks;
    g_desc[4].tag = SEL_TAG_END;
    g_desc[4].value = 0;
    return g_desc;
}
