/* I652d: PLAYLIST_ADD_FOLDER scans a folder recursively with the library folder scan and appends every recognised module to a saved
 * playlist in path (strcmp) order. Driven through the engine command entry (send + pump), read back from the playlist keys and
 * sel_engine_stats_get() (addf_runs sentinel, addf_added). The scan is asynchronous: pump until addf_runs moves. A file that is not a
 * module is skipped; a missing folder adds nothing. argv[1] is the gen_testmod tool, which writes the real modules. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void run(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
}

static sel_engine_stats addf_stats(sel_engine* e) {
    sel_engine_stats s;
    sel_engine_stats_get(e, &s);
    return s;
}

static void write_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static void gen(const char* tool, const char* flag, const char* path) {
    char cmd[700];
    snprintf(cmd, sizeof cmd, "\"%s\" %s \"%s\"", tool, flag, path);
    CHECK(system(cmd) == 0);
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_pladdf_XXXXXX";
    char db[256], tree[256], p0[300], p1[300], p2[300], txt[300], cmd[700], line[700], keys[2048];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t n = 0;
    time_t t0;
    if (argc < 2 || mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    snprintf(tree, sizeof tree, "%s/tree", dir);
    /* Path order is m/b.mod, sub/deep/a.mod, z.mod; a directory walk would not give it. */
    snprintf(cmd, sizeof cmd, "mkdir -p \"%s/sub/deep\" \"%s/m\"", tree, tree);
    CHECK(system(cmd) == 0);
    snprintf(p0, sizeof p0, "%s/m/b.mod", tree);
    snprintf(p1, sizeof p1, "%s/sub/deep/a.mod", tree);
    snprintf(p2, sizeof p2, "%s/z.mod", tree);
    snprintf(txt, sizeof txt, "%s/sub/readme.txt", tree);
    gen(argv[1], "", p2);
    gen(argv[1], "--short", p1);
    gen(argv[1], "--loop", p0);
    write_file(txt, "not a module");

    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    run(e, "PLAYLIST_CREATE alpha");

    CHECK(addf_stats(e).addf_runs == 0u);
    snprintf(line, sizeof line, "PLAYLIST_ADD_FOLDER 1 \"%s\"", tree);
    run(e, line);
    t0 = time(NULL);
    while (addf_stats(e).addf_runs == 0u && time(NULL) - t0 < 30) {
        sel_engine_pump(e, NULL, NULL);
        usleep(10000);
    }
    CHECK(addf_stats(e).addf_runs == 1u); /* the add-folder code ran */
    CHECK(addf_stats(e).addf_added == 3u);
    CHECK(sel_library_playlist_keys(lib, 1u, keys, sizeof keys, &n) == 0);
    CHECK(n == 3u);
    CHECK(strcmp(keys, p0) == 0); /* NUL terminated keys in path order */
    CHECK(strcmp(keys + strlen(p0) + 1u, p1) == 0);
    CHECK(strcmp(keys + strlen(p0) + strlen(p1) + 2u, p2) == 0);

    snprintf(line, sizeof line, "PLAYLIST_ADD_FOLDER 1 \"%s/nothere\"", dir);
    run(e, line);
    for (int i = 0; i < 100; i++) {
        sel_engine_pump(e, NULL, NULL);
        usleep(10000);
    }
    CHECK(addf_stats(e).addf_runs == 2u && addf_stats(e).addf_added == 0u); /* a missing folder finishes the run and adds nothing */
    CHECK(sel_library_playlist_keys(lib, 1u, keys, sizeof keys, &n) == 0 && n == 3u);

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("playlist_add_folder: %d failures\n", g_fail);
        return 1;
    }
    printf("playlist_add_folder_ok\n");
    return 0;
}
