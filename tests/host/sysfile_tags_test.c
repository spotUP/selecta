/* I710ga1: sel_registry_sysfiles parses RP_SYSFILE declarations from a plugin's hook list. A plugin without the tag
 * yields none; entries missing NAME, LABEL or SHA256 (or with a bad name or digest) are dropped; unknown SYSFILE tags
 * inside an entry are skipped. Hand-built tag lists. */
#include <selecta/registry.h>
#include <selecta/replayer.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            failures++;                                           \
            printf("FAIL line %d: %s\n", __LINE__, #c);           \
        }                                                         \
    } while (0)

#define SHA_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define SHA_B "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

static void set(sel_tag* t, uint32_t tag, uintptr_t v) {
    t->tag = tag;
    t->value = v;
}

int main(void) {
    sel_tag good[8], noname[4], badsha[5], badname[5], unknown[8], hooks[6], none[2];
    sel_sysfile_decl out[SEL_SYSFILE_MAX];
    unsigned n;

    set(&good[0], SEL_RP_SYSFILE_NAME, (uintptr_t) "cybertracker101.d64");
    set(&good[1], SEL_RP_SYSFILE_LABEL, (uintptr_t) "CyberTracker disk image");
    set(&good[2], SEL_RP_SYSFILE_HINT, (uintptr_t) "Find it on CSDb.");
    set(&good[3], SEL_RP_SYSFILE_SHA256, (uintptr_t)(SHA_A ";" SHA_B));
    set(&good[4], SEL_RP_SYSFILE_URLS, (uintptr_t) "http://a.test/x.zip;http://b.test/x.d64");
    set(&good[5], SEL_RP_SYSFILE_SIZE, 174848);
    set(&good[6], SEL_TAG_END, 0);

    set(&noname[0], SEL_RP_SYSFILE_LABEL, (uintptr_t) "x");
    set(&noname[1], SEL_RP_SYSFILE_SHA256, (uintptr_t) SHA_A);
    set(&noname[2], SEL_TAG_END, 0);

    set(&badsha[0], SEL_RP_SYSFILE_NAME, (uintptr_t) "a.bin");
    set(&badsha[1], SEL_RP_SYSFILE_LABEL, (uintptr_t) "x");
    set(&badsha[2], SEL_RP_SYSFILE_SHA256, (uintptr_t) "ABC");
    set(&badsha[3], SEL_TAG_END, 0);

    set(&badname[0], SEL_RP_SYSFILE_NAME, (uintptr_t) "../etc/passwd");
    set(&badname[1], SEL_RP_SYSFILE_LABEL, (uintptr_t) "x");
    set(&badname[2], SEL_RP_SYSFILE_SHA256, (uintptr_t) SHA_A);
    set(&badname[3], SEL_TAG_END, 0);

    set(&unknown[0], SEL_RP_SYSFILE_NAME, (uintptr_t) "rom.bin");
    set(&unknown[1], SEL_RP_SYSFILE_LABEL, (uintptr_t) "Kickstart");
    set(&unknown[2], SEL_RP_SYSFILE_SHA256, (uintptr_t) SHA_B);
    set(&unknown[3], SEL_TAG_USER | 1u, 5); /* a field a later host adds */
    set(&unknown[4], SEL_TAG_END, 0);

    /* one good declaration: 2 sha256, 2 URLs */
    set(&hooks[0], SEL_RP_SYSFILE, (uintptr_t)good);
    set(&hooks[1], SEL_TAG_END, 0);
    n = sel_registry_sysfiles(hooks, out, SEL_SYSFILE_MAX);
    CHECK(n == 1);
    CHECK(strcmp(out[0].name, "cybertracker101.d64") == 0);
    CHECK(strcmp(out[0].label, "CyberTracker disk image") == 0);
    CHECK(strcmp(out[0].hint, "Find it on CSDb.") == 0);
    CHECK(strcmp(out[0].sha256, SHA_A ";" SHA_B) == 0);
    CHECK(strcmp(out[0].urls, "http://a.test/x.zip;http://b.test/x.d64") == 0);
    CHECK(out[0].size == 174848);

    /* no tag: none */
    set(&none[0], SEL_RP_NAME, (uintptr_t) "x");
    set(&none[1], SEL_TAG_END, 0);
    CHECK(sel_registry_sysfiles(none, out, SEL_SYSFILE_MAX) == 0);
    CHECK(sel_registry_sysfiles(NULL, out, SEL_SYSFILE_MAX) == 0);

    /* malformed entries are rejected, a good one beside them survives; entries repeat */
    set(&hooks[0], SEL_RP_SYSFILE, (uintptr_t)noname);
    set(&hooks[1], SEL_RP_SYSFILE, (uintptr_t)badsha);
    set(&hooks[2], SEL_RP_SYSFILE, (uintptr_t)badname);
    set(&hooks[3], SEL_RP_SYSFILE, (uintptr_t)unknown);
    set(&hooks[4], SEL_RP_SYSFILE, (uintptr_t)good);
    set(&hooks[5], SEL_TAG_END, 0);
    n = sel_registry_sysfiles(hooks, out, SEL_SYSFILE_MAX);
    CHECK(n == 2);
    CHECK(n == 2 && strcmp(out[0].name, "rom.bin") == 0 && out[0].size == 0 && out[0].hint[0] == 0);
    CHECK(n == 2 && strcmp(out[1].name, "cybertracker101.d64") == 0);
    /* max bounds the output */
    CHECK(sel_registry_sysfiles(hooks, out, 1) == 1);

    if (failures == 0) {
        printf("sysfile_tags_ok\n");
    }
    return failures != 0;
}
