/* I652c: PLAYLIST_IMPORT parses an .m3u with the playlist codecs and appends each existing file to a saved playlist in file
 * order; an entry whose file is missing is counted, not added. Driven through the engine's command entry (sel_engine_send +
 * pump) and read back from the library (the playlist keys) and sel_engine_stats_get() (import_runs sentinel, import_added,
 * import_missing). A file that is not a playlist leaves the playlist untouched. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
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

static sel_engine_stats imp_stats(sel_engine* e) {
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

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_plimport_XXXXXX";
    char db[256], a[256], b[256], gone[256], m3u[256], text[1200], line[600], keys[1024];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t id = 0, n = 0;
    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    snprintf(a, sizeof a, "%s/a.mod", dir);
    snprintf(b, sizeof b, "%s/b.mod", dir);
    snprintf(gone, sizeof gone, "%s/gone.mod", dir);
    snprintf(m3u, sizeof m3u, "%s/list.m3u", dir);
    write_file(a, "a");
    write_file(b, "b");
    snprintf(text, sizeof text, "#EXTM3U\n%s\n%s\nb.mod\n", a, gone); /* absolute, absolute-missing, relative to the list */
    write_file(m3u, text);

    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    run(e, "PLAYLIST_CREATE alpha");
    id = 1u;

    CHECK(imp_stats(e).import_runs == 0u);
    snprintf(line, sizeof line, "PLAYLIST_IMPORT %u \"%s\"", (unsigned)id, m3u);
    run(e, line);
    CHECK(imp_stats(e).import_runs == 1u); /* the import code ran */
    CHECK(imp_stats(e).import_added == 2u);
    CHECK(imp_stats(e).import_missing == 1u);
    CHECK(sel_library_playlist_keys(lib, id, keys, sizeof keys, &n) == 0);
    CHECK(n == 2u);
    CHECK(strcmp(keys, a) == 0); /* file order: the NUL terminated keys are a, then b */
    CHECK(strcmp(keys + strlen(a) + 1u, b) == 0);

    snprintf(line, sizeof line, "PLAYLIST_IMPORT %u \"%s/nothere.m3u\"", (unsigned)id, dir);
    run(e, line);
    CHECK(imp_stats(e).import_runs == 1u); /* an unreadable file imports nothing */
    CHECK(imp_stats(e).pl_fail_runs == 1u && imp_stats(e).pl_fail_reason == SEL_PLFAIL_UNREADABLE); /* and reports the failure (I652e) */
    CHECK(sel_library_playlist_keys(lib, id, keys, sizeof keys, &n) == 0 && n == 2u);

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(line, sizeof line, "rm -rf \"%s\"", dir);
    (void)system(line);
    if (g_fail) {
        printf("playlist_import: %d failures\n", g_fail);
        return 1;
    }
    printf("playlist_import_ok\n");
    return 0;
}
