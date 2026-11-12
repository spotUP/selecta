/* I427e5: the owner of a replayer's hooks is the PLUGIN_ID of the registry entry that lists them. A manifest plugin
 * (SEL_PLUGIN_MODULE) is listed with its manifest hooks; once its module is loaded the engine holds the module's real
 * hooks, and the owner must still be the manifest's id (the engine's system-file declarations carry it), not "?".
 * A fake resolver stands in for the Amiga loader (src/engine/loader_seg.c); the registry is a hand-built list. */
#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/replayer.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            failures++;                                 \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
        }                                               \
    } while (0)

#define SHA_B "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

static const sel_tag k_entry[] = {{SEL_RP_SYSFILE_NAME, (uintptr_t) "rom.bin"},
                                  {SEL_RP_SYSFILE_LABEL, (uintptr_t) "Kickstart"},
                                  {SEL_RP_SYSFILE_SHA256, (uintptr_t)SHA_B},
                                  {SEL_TAG_END, 0}};
/* the module's real hooks, as its sel_plugin_entry hands them out */
static const sel_tag k_real[] = {{SEL_RP_NAME, (uintptr_t) "Module test"}, {SEL_RP_SYSFILE, (uintptr_t)k_entry}, {SEL_TAG_END, 0}};
/* the manifest's hooks, as the registry lists them */
static const sel_tag k_manifest[] = {{SEL_RP_NAME, (uintptr_t) "Module test"}, {SEL_TAG_END, 0}};
static const sel_tag k_plain[] = {{SEL_RP_NAME, (uintptr_t) "Plain"}, {SEL_TAG_END, 0}};
static const sel_tag k_stray[] = {{SEL_RP_NAME, (uintptr_t) "Stray"}, {SEL_TAG_END, 0}};

static const sel_tag k_desc_plain[] = {{SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                                       {SEL_PLUGIN_ID, (uintptr_t) "test.plain"},
                                       {SEL_PLUGIN_TAGS, (uintptr_t)k_plain},
                                       {SEL_TAG_END, 0}};
static const sel_tag k_desc_module[] = {{SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                                        {SEL_PLUGIN_ID, (uintptr_t) "test.module"},
                                        {SEL_PLUGIN_MODULE, (uintptr_t) "sel_test"},
                                        {SEL_PLUGIN_TAGS, (uintptr_t)k_manifest},
                                        {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return 2;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0 ? k_desc_plain : k_desc_module;
}

static const sel_tag* fake_resolve(const sel_tag* hooks) {
    return hooks == k_manifest ? k_real : hooks;
}

static const sel_tag* fake_manifest_of(const sel_tag* hooks) {
    return hooks == k_real ? k_manifest : hooks;
}

int main(void) {
    sel_sysfile_decl d[SEL_SYSFILE_MAX];
    const sel_tag* real;
    unsigned n;

    sel_plugin_set_resolver(fake_resolve, fake_manifest_of);
    real = sel_plugin_resolve(k_manifest);
    CHECK(real == k_real);
    CHECK(sel_plugin_manifest(real) == k_manifest);
    CHECK(sel_plugin_manifest(k_plain) == k_plain);
    CHECK(strcmp(sel_plugin_owner_id(reg_count, reg_get, real), "test.module") == 0);
    CHECK(strcmp(sel_plugin_owner_id(reg_count, reg_get, k_manifest), "test.module") == 0);
    CHECK(strcmp(sel_plugin_owner_id(reg_count, reg_get, k_plain), "test.plain") == 0);
    CHECK(strcmp(sel_plugin_owner_id(reg_count, reg_get, k_stray), "?") == 0);
    CHECK(strcmp(sel_plugin_owner_id(NULL, NULL, real), "?") == 0);

    /* what the engine does before RP_OPEN (engine.c sysfiles_ready): declarations from the real hooks, owner by id */
    n = sel_sysfile_decls(real, sel_plugin_owner_id(reg_count, reg_get, real), d, SEL_SYSFILE_MAX, NULL, NULL);
    CHECK(n == 1);
    CHECK(n == 1 && strcmp(d[0].plugin, "test.module") == 0);

    /* no loader installed: hooks come back unchanged */
    sel_plugin_set_resolver(NULL, NULL);
    CHECK(sel_plugin_manifest(k_real) == k_real);

    if (failures == 0) {
        printf("plugin_owner_ok\n");
    }
    return failures != 0;
}
