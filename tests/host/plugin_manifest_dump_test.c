/* I1193b-M2: selecta-cli plugins --manifest-c writes generated C from the tags.def type table. Two plugin
 * descriptors are dumped, the C is compiled into a shared object with the build's own compiler, loaded, and every
 * INT, STR and LIST tag must come back equal; pointer tags must be gone from the lists and named, as ids, under
 * SEL_PLUGIN_HOOKS_PRESENT. */
#include "../../src/core/manifest_dump.h"

#include <selecta/plugin.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            failures++;                                 \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
        }                                               \
    } while (0)

static int probe_stub(const void* a) {
    (void)a;
    return 1;
}

static const sel_tag k_hooks_a[] = {{SEL_RP_NAME, (uintptr_t) "Alpha \"quoted\" \\ name"},
                                    {SEL_RP_VERSION, 7},
                                    {SEL_RP_EXTENSIONS, (uintptr_t) "aa,ab"},
                                    {SEL_RP_PROBE, (uintptr_t)probe_stub},
                                    {SEL_TAG_END, 0}};
static const sel_tag k_hooks_b[] = {{SEL_RP_NAME, (uintptr_t) "Beta"},
                                    {SEL_RP_MAX_VOICES, 4},
                                    {SEL_TAG_END, 0}};
static const sel_tag k_desc_a[] = {{SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                                   {SEL_PLUGIN_ID, (uintptr_t) "test.alpha"},
                                   {SEL_PLUGIN_VERSION, 3},
                                   {SEL_PLUGIN_TAGS, (uintptr_t)k_hooks_a},
                                   {SEL_TAG_END, 0}};
static const sel_tag k_desc_b[] = {{SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                                   {SEL_PLUGIN_ID, (uintptr_t) "test.beta"},
                                   {SEL_PLUGIN_VERSION, 9},
                                   {SEL_PLUGIN_TAGS, (uintptr_t)k_hooks_b},
                                   {SEL_TAG_END, 0}};

static int present(const sel_tag* list, uint32_t tag) {
    return list != NULL && sel_tag_find(list, tag, 99) == 0;
}

static void same_value_tags(const sel_tag* want, const sel_tag* got) {
    const sel_tag* it = want;
    const sel_tag* t;
    sel_tag_type ty;
    while ((t = sel_tag_next(&it)) != NULL) {
        if (!sel_tag_type_of(t->tag, &ty)) {
            CHECK(0);
        } else if (ty == SEL_TT_INT) {
            CHECK(sel_tag_find(got, t->tag, 12345) == t->value);
        } else if (ty == SEL_TT_STR) {
            const char* g = (const char*)sel_tag_find(got, t->tag, 0);
            CHECK(g != NULL && strcmp(g, (const char*)t->value) == 0);
        } else if (ty == SEL_TT_PTR) {
            CHECK(sel_tag_find(got, t->tag, 12345) == 12345); /* no pointer in a manifest */
        }
    }
}

int main(void) {
    const sel_tag* descs[2] = {k_desc_a, k_desc_b};
    const char* mods[2] = {"sel_alpha", "sel_beta"};
    char cmd[1024];
    void* lib;
    sel_plugin_entry_fn* entries;
    FILE* f = fopen("manifest_dump_test.c", "w");
    int i;

    CHECK(f != NULL);
    if (f == NULL) {
        return 1;
    }
    CHECK(sel_manifest_dump_c(f, descs, mods, 2) == 0);
    fclose(f);
    snprintf(cmd, sizeof cmd, "\"%s\" -std=c99 -Wall -Wextra -Werror -shared -fPIC -I\"%s\" manifest_dump_test.c -o manifest_dump_test.so",
             SEL_TEST_CC, SEL_TEST_INC);
    CHECK(system(cmd) == 0);
    lib = dlopen("./manifest_dump_test.so", RTLD_NOW);
    CHECK(lib != NULL);
    entries = lib != NULL ? (sel_plugin_entry_fn*)dlsym(lib, "sel_manifest_entries") : NULL;
    CHECK(entries != NULL);
    for (i = 0; entries != NULL && i < 2; i++) {
        const sel_tag* want = descs[i];
        const sel_tag* got = entries[i] != NULL ? entries[i](SEL_API_VERSION) : NULL;
        const sel_tag* wh = (const sel_tag*)sel_tag_find(want, SEL_PLUGIN_TAGS, 0);
        const sel_tag* gh;
        const sel_tag* pres;
        CHECK(got != NULL);
        if (got == NULL) {
            continue;
        }
        same_value_tags(want, got);
        CHECK(strcmp((const char*)sel_tag_find(got, SEL_PLUGIN_MODULE, 0), mods[i]) == 0);
        gh = (const sel_tag*)sel_tag_find(got, SEL_PLUGIN_TAGS, 0);
        CHECK(gh != NULL);
        same_value_tags(wh, gh);
        pres = (const sel_tag*)sel_tag_find(got, SEL_PLUGIN_HOOKS_PRESENT, 0);
        CHECK(i == 0 ? present(pres, SEL_RP_PROBE) : pres == NULL);
    }
    CHECK(entries != NULL && entries[2] == NULL);
    if (lib != NULL) {
        dlclose(lib);
    }
    remove("manifest_dump_test.c");
    remove("manifest_dump_test.so");
    if (failures == 0) {
        printf("plugin_manifest_dump_ok\n");
    }
    return failures != 0;
}
