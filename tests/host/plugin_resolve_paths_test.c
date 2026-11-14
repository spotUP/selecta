/* I1193b-M1: every read of a pointer tag from a registry descriptor goes through sel_plugin_resolve. A stub source
 * plugin is listed with MANIFEST hooks that carry no SRC_DESCRIPTOR (what a lazy-loaded descriptor looks like, M4);
 * the fake resolver hands out the real hooks and counts the calls. If source registration read the pointer tag
 * from the manifest hooks, the source would not register and the resolver would never run. sel_hooks_has answers
 * presence on the manifest hooks without calling the resolver. */
#include "../../src/engine/claims.h"
#include "../../src/library/source.h"

#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/source.h>

#include <stdio.h>
#include <string.h>

static int failures;
static int resolves;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            failures++;                                 \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
        }                                               \
    } while (0)

static const sel_source k_src = {.id = "stubsrc", .name = "Stub source"};
static const sel_tag k_real[] = {{SEL_SRC_DESCRIPTOR, (uintptr_t)&k_src}, {SEL_TAG_END, 0}};
static const sel_tag k_manifest[] = {{SEL_TAG_END, 0}};
static const sel_tag k_desc[] = {{SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_SOURCE},
                                 {SEL_PLUGIN_ID, (uintptr_t) "test.stubsrc"},
                                 {SEL_PLUGIN_MODULE, (uintptr_t) "sel_stub"},
                                 {SEL_PLUGIN_TAGS, (uintptr_t)k_manifest},
                                 {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return 1;
}
static const sel_tag* reg_get(uint32_t i) {
    (void)i;
    return k_desc;
}
static const sel_tag* fake_resolve(const sel_tag* hooks) {
    resolves++;
    return hooks == k_manifest ? k_real : hooks;
}
static const sel_tag* fake_manifest_of(const sel_tag* hooks) {
    return hooks == k_real ? k_manifest : hooks;
}

int main(void) {
    sel_plugin_set_resolver(fake_resolve, fake_manifest_of);
    CHECK(!sel_hooks_has(k_manifest, SEL_SRC_DESCRIPTOR));
    CHECK(sel_hooks_has(k_real, SEL_SRC_DESCRIPTOR));
    CHECK(!sel_hooks_has(NULL, SEL_SRC_DESCRIPTOR));
    CHECK(resolves == 0); /* presence needs no module */

    sel_source_set_plugins(reg_count, reg_get, NULL);
    sel_source_register_builtin();
    CHECK(resolves > 0);
    CHECK(sel_source_find("stubsrc") == &k_src);

    if (failures == 0) {
        printf("plugin_resolve_paths_ok\n");
    }
    return failures != 0;
}
