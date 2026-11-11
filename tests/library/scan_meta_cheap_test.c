/* I1314: the library scan describes a song from a manifest's SEL_RP_SCAN_META hook, without opening the replayer,
 * only when the manifest's own probe answered YES and the hook returned a list. A fake replayer claims three files
 * through LIBRARY_SCAN (the engine's real scan path, src/library/sources/local.c scan_blob_ex):
 *   sure.chp   probe YES, hook returns FORMAT "Cheap Sure"   -> row from the hook, no open (scan_meta_cheap 1)
 *   ext.chp    probe UNSURE (extension-only claim)           -> opened, FORMAT "Opened" from RP_METADATA
 *   null.chp   probe YES, hook returns NULL                  -> opened, FORMAT "Opened"
 * Checks: opens == 2, hook calls == 2 (never for the UNSURE file), scan_meta_cheap == 1, and the stored formats.
 * I1314a: then OPENs sure.chp (row name is the file-name placeholder, artist preset) with the player reporting title, author and length;
 * the play-time write fills name and duration_ms, keeps the preset artist, and moves engine stats meta_fills to 1.
 * Prints scan_meta_cheap_ok. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "../engine/engine_test_db_lock.h"

static int g_fail;
#define CHECK(c)                                                                                                       \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                       \
            g_fail = 1;                                                                                                \
        }                                                                                                              \
    } while (0)

static unsigned g_opens, g_hook_calls;
static int g_state;

static int chp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t n = name != NULL ? strlen(name) : 0u;
    if (size >= 4u && (memcmp(data, "SURE", 4) == 0 || memcmp(data, "NULL", 4) == 0)) {
        return SEL_PROBE_YES;
    }
    return n >= 4u && strcmp(name + n - 4u, ".chp") == 0 ? SEL_PROBE_UNSURE : SEL_PROBE_NO;
}

static const sel_tag* chp_scan_meta(const uint8_t* data, size_t size, const char* path, char* buf, size_t buf_n) {
    static sel_tag meta[2];
    (void)path;
    g_hook_calls++;
    if (size < 4u || memcmp(data, "SURE", 4) != 0 || buf_n < 16u) {
        return NULL;
    }
    snprintf(buf, buf_n, "Cheap Sure");
    meta[0].tag = SEL_META_FORMAT;
    meta[0].value = (uintptr_t)buf;
    meta[1].tag = SEL_TAG_END;
    meta[1].value = 0;
    return meta;
}

static int g_playing; /* I1314a: set before the play phase; the player then reports title, author and length */

static void* chp_open(const sel_song_source* src, const sel_host* h, sel_rp_info* info) {
    (void)src;
    (void)h;
    info->subsong_count = 1;
    info->default_subsong = 0;
    g_opens++;
    return &g_state;
}

static void chp_close(void* state) {
    (void)state;
}

static const sel_tag* chp_metadata(void* state) {
    static const sel_tag meta[] = {{SEL_META_FORMAT, (uintptr_t) "Opened"}, {SEL_TAG_END, 0}};
    static const sel_tag played[] = {{SEL_META_FORMAT, (uintptr_t) "Opened"},
                                     {SEL_META_TITLE, (uintptr_t) "Played Title"},
                                     {SEL_META_AUTHOR, (uintptr_t) "Player Author"},
                                     {SEL_META_DURATION_MS, 123000},
                                     {SEL_TAG_END, 0}};
    (void)state;
    return g_playing ? played : meta;
}

static int chp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    (void)state;
    (void)subsong;
    (void)rate;
    (void)voices;
    return 1;
}

static uint32_t chp_render(void* state, int16_t* out, uint32_t frames) {
    (void)state;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

static sel_tag g_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},           {SEL_RP_EXTENSIONS, (uintptr_t) "chp"},
                            {SEL_RP_PROBE, SEL_TAG_FN(chp_probe)},       {SEL_RP_SCAN_META, SEL_TAG_FN(chp_scan_meta)},
                            {SEL_RP_OPEN, SEL_TAG_FN(chp_open)},         {SEL_RP_CLOSE, SEL_TAG_FN(chp_close)},
                            {SEL_RP_METADATA, SEL_TAG_FN(chp_metadata)}, {SEL_RP_START, SEL_TAG_FN(chp_start)},
                            {SEL_RP_RENDER, SEL_TAG_FN(chp_render)},     {SEL_TAG_END, 0}};
static sel_tag g_desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_cheap"},
                           {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                           {SEL_PLUGIN_TAGS, (uintptr_t)g_hooks},
                           {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return 1u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0u ? g_desc : NULL;
}

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static void write_file(const char* dir, const char* name, const char* body) {
    char path[512];
    FILE* f;
    snprintf(path, sizeof path, "%s/songs/%s", dir, name);
    f = fopen(path, "wb");
    if (f != NULL) {
        fputs(body, f);
        fclose(f);
    }
}

/* One column of the songs row of `name` as text ("" when NULL or no row). */
static void read_field(sel_library* lib, const char* col, const char* name, char* got, size_t cap) {
    sqlite3_stmt* st = NULL;
    char sql[96];
    got[0] = '\0';
    snprintf(sql, sizeof sql, "SELECT %s FROM songs WHERE path LIKE ?", col);
    db_enter(lib);
    if (sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL) == SQLITE_OK) {
        char like[128];
        snprintf(like, sizeof like, "%%/%s", name);
        sqlite3_bind_text(st, 1, like, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0) != NULL) {
            snprintf(got, cap, "%s", (const char*)sqlite3_column_text(st, 0));
        }
    }
    sqlite3_finalize(st);
    db_leave(lib);
}

static void check_field(sel_library* lib, const char* col, const char* name, const char* want) {
    char got[512];
    read_field(lib, col, name, got, sizeof got);
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: %s \"%s\", want \"%s\"\n", name, col, got, want);
        g_fail = 1;
    }
}

static void check_format(sel_library* lib, const char* name, const char* want) {
    check_field(lib, "format", name, want);
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_scanmeta_XXXXXX";
    char db[256], songs[256], line[512], pool[1024];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_scan_info si;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    snprintf(songs, sizeof songs, "%s/songs", dir);
    mkdir(songs, 0755);
    write_file(dir, "sure.chp", "SURE song data");
    write_file(dir, "ext.chp", "plain song data");
    write_file(dir, "null.chp", "NULL song data");
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    if (lib == NULL) return 1;
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    if (e == NULL) return 1;
    CHECK(sel_engine_send_line(e, "PREF_SET library.sources_off \"hippo\"", pool, sizeof pool) == 0);
    setenv("SELECTA_ART_SCRAPE", "off", 1); /* no network from a test scan */
    snprintf(line, sizeof line, "LIBRARY_SCAN %s", songs);
    CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
    sel_engine_pump(e, sink, NULL);
    sel_engine_scan_wait(e, sink, NULL);
    sel_engine_scan_info(e, &si);
    printf("scanned_files=%u skipped_files=%u scan_meta_cheap=%u opens=%u hook_calls=%u\n", (unsigned)si.scanned_files,
           (unsigned)si.skipped_files, (unsigned)si.scan_meta_cheap, g_opens, g_hook_calls);
    CHECK(si.scan_meta_cheap == 1u); /* sentinel: the cheap branch ran exactly for sure.chp */
    CHECK(g_opens == 2u);            /* ext.chp (UNSURE) and null.chp (hook said NULL) still open */
    CHECK(g_hook_calls == 2u);       /* the hook never runs for an UNSURE claim */
    check_format(lib, "sure.chp", "Cheap Sure");
    check_format(lib, "ext.chp", "Opened");
    check_format(lib, "null.chp", "Opened");
    /* I1314a: the signature-described row has no title, author or length until the song plays; playing it fills the empty
     * fields from the player and keeps a field that already has a value (the artist set here stands for a tag edit). */
    check_field(lib, "name", "sure.chp", "sure.chp"); /* the scan's file-name placeholder */
    check_field(lib, "duration_ms", "sure.chp", "");
    db_enter(lib);
    CHECK(sqlite3_exec(lib->db, "UPDATE songs SET artist = 'Kept Author' WHERE path LIKE '%/sure.chp'", NULL, NULL, NULL) == SQLITE_OK);
    db_leave(lib);
    {
        char path[512];
        sel_engine_stats s1;
        int i;

        read_field(lib, "path", "sure.chp", path, sizeof path);
        g_playing = 1;
        snprintf(line, sizeof line, "OPEN \"%s\"", path);
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        for (i = 0; i < 2500; i++) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &s1);
            if (s1.meta_fills > 0u) break;
            usleep(2000);
        }
        printf("play path=%s meta_fills=%u\n", path, (unsigned)s1.meta_fills);
        CHECK(s1.meta_fills == 1u); /* sentinel: the play-time write changed the row */
        check_field(lib, "name", "sure.chp", "Played Title");
        check_field(lib, "artist", "sure.chp", "Kept Author");
        check_field(lib, "duration_ms", "sure.chp", "123000");
    }
    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail) return 1;
    printf("scan_meta_cheap_ok\n");
    return 0;
}
