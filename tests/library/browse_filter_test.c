/* Regression (I218i): the folder browser listed files that are not songs, and selecting one failed to play.
 * Driven through the engine (LIBRARY_BROWSE, VIEW_SHOW browse) with a registry the test controls, so it
 * proves the replayers' declared SEL_RP_EXTENSIONS is what makes a file visible: an extension only one
 * plugin declares is shown, an unknown one hidden, an Amiga prefix name shown, folders, playlists and
 * archives stay, case variants match, and a registry without the plugin hides the file again.
 * Sentinel: the registry must have been consulted while listing. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

/* Registry A: one replayer declaring "zzq" (no other plugin has it) and the prefix "pfx".
 * Registry B: a replayer declaring only "mod". Selected by g_set. */
static sel_tag g_hooks_a[] = {{SEL_RP_EXTENSIONS, (uintptr_t) "mod,ZZQ,pfx"}, {SEL_TAG_END, 0}};
static sel_tag g_hooks_b[] = {{SEL_RP_EXTENSIONS, (uintptr_t) "mod"}, {SEL_TAG_END, 0}};
static sel_tag g_desc_a[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_a"},
                             {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                             {SEL_PLUGIN_TAGS, (uintptr_t)g_hooks_a},
                             {SEL_TAG_END, 0}};
static sel_tag g_desc_b[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_b"},
                             {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                             {SEL_PLUGIN_TAGS, (uintptr_t)g_hooks_b},
                             {SEL_TAG_END, 0}};
static int g_set;
static uint32_t g_calls;

static uint32_t reg_count(void) {
    g_calls++;
    return 1u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0u ? (g_set == 0 ? g_desc_a : g_desc_b) : NULL;
}

static sel_engine* eng;

static void run(const char* line) {
    sel_message m;
    char pool[1536];

    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
}

static int shown(const char* name) {
    const sel_view_state* v = sel_engine_state(eng)->view;
    uint32_t i;

    for (i = 0; v != NULL && i < v->row_n; i++) {
        if (strcmp(v->rows[i].text, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void touch(const char* dir, const char* name) {
    char p[1024];
    FILE* f;

    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (f != NULL) {
        fputs("x", f);
        fclose(f);
    }
}

static void list(const char* dir) {
    char cmd[1200];

    snprintf(cmd, sizeof cmd, "LIBRARY_BROWSE %s", dir);
    run(cmd);
    run("VIEW_SHOW browse");
}

int main(void) {
    char dir[] = "/tmp/selecta_browse_filter_XXXXXX";
    char sub[1024];
    const sel_host* host = sel_host_default();
    sel_engine_config cfg;
    uint32_t before;

    CHECK(mkdtemp(dir) != NULL);
    snprintf(sub, sizeof sub, "%s/Folder", dir);
    CHECK(mkdir(sub, 0755) == 0);
    touch(dir, "readme.txt");
    touch(dir, "cover.jpg");
    touch(dir, "noextension");
    touch(dir, "song.mod");
    touch(dir, "LOUD.MoD");
    touch(dir, "tune.zzq"); /* extension only registry A declares */
    touch(dir, "TUNE2.Zzq");
    touch(dir, "pfx.intro"); /* Amiga prefix form */
    touch(dir, "PFX.Outro");
    touch(dir, "other.xyz");
    touch(dir, "list.m3u");
    touch(dir, "pack.lha");

    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    eng = sel_engine_create(host, &cfg);
    CHECK(eng != NULL);
    if (eng == NULL) {
        return 2;
    }

    before = g_calls;
    list(dir);
    CHECK(g_calls > before); /* the engine's predicate consulted the registry */
    CHECK(!shown("readme.txt")); /* unknown file hidden */
    CHECK(!shown("cover.jpg"));
    CHECK(!shown("noextension"));
    CHECK(!shown("other.xyz"));
    CHECK(shown("song.mod"));
    CHECK(shown("LOUD.MoD")); /* case variant */
    CHECK(shown("tune.zzq")); /* declared by one plugin only */
    CHECK(shown("TUNE2.Zzq"));
    CHECK(shown("pfx.intro")); /* Amiga prefix */
    CHECK(shown("PFX.Outro"));
    CHECK(shown("Folder")); /* folder shown */
    CHECK(shown("list.m3u")); /* playlist stays */
    CHECK(shown("pack.lha")); /* archive stays */

    g_set = 1; /* the plugin that declared zzq and pfx is gone: those files go with it */
    list(dir);
    CHECK(shown("song.mod") && !shown("tune.zzq") && !shown("pfx.intro") && shown("Folder") && shown("list.m3u"));

    sel_engine_destroy(eng);
    if (g_fail == 0) {
        printf("browse_filter_ok\n");
    }
    return g_fail != 0;
}
