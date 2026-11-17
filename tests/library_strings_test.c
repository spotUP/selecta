/* I774p: the idle pass stores a replayer's sample/instrument names and song message in song_strings; the text: term finds
 * them through FTS5 MATCH and, with FTS5 reported absent, through LIKE. The replayer is a fake plugin with an RP_STRINGS
 * hook (the openmpt plugin is a loadable module and is never linked into a test). */
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/replayer.h>
#include "db.h"
#include "db_internal.h"
#include "query.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

static int g_fail;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL: %s\n", what);
        g_fail = 1;
    }
}

static int probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 4u && memcmp(data, "STRG", 4) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static size_t strings(const uint8_t* data, size_t size, char* buf, size_t buf_n) {
    (void)size;
    return (size_t)snprintf(buf, buf_n, "Fixture Lead\nacid_bass\n%s\n", (const char*)data + 4);
}

static sel_tag g_hooks[3];
static sel_tag g_desc[4];

static const sel_tag* g_reg = g_desc;

static uint32_t reg_count(void) {
    return 1u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0u ? g_reg : NULL;
}

static void put_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");

    if (f == NULL) exit(2);
    fputs(text, f);
    fclose(f);
}

static void scan(sel_library* lib, const char* path) {
    sel_scan_row row;
    sel_scan_batch b;
    sel_message reply;

    memset(&row, 0, sizeof row);
    row.source = "local";
    row.path = path;
    row.name = path;
    memset(&b, 0, sizeof b);
    b.rows = &row;
    b.count = 1;
    b.now = 1;
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "scan batch");
}

static int query_rows(sel_library* lib, const char* text) {
    sel_query q;
    sqlite3_stmt* st = NULL;
    unsigned i;
    int n = 0;

    if (!sel_query_compile(text, 1, NULL, NULL, &q)) return -1;
    if (sqlite3_prepare_v2(lib->db, q.sql, -1, &st, NULL) != SQLITE_OK) return -1;
    for (i = 0; i < q.nparams; i++) {
        if (q.params[i].is_int) sqlite3_bind_int64(st, (int)i + 1, q.params[i].i);
        else sqlite3_bind_text(st, (int)i + 1, q.params[i].s, -1, SQLITE_TRANSIENT);
    }
    while (sqlite3_step(st) == SQLITE_ROW) n++;
    sqlite3_finalize(st);
    return n;
}

/* Optional argv: <openmpt module> <real .mod>. The real adapter rp_strings runs on a real module; its sample names must land in song_strings. */
static void real_module(const char* modpath, const char* song) {
    char root[] = "/tmp/selecta_strings_real_XXXXXX";
    char db[320];
    typedef const sel_tag* (*entry_fn)(uint32_t);
    void* h = dlopen(modpath, RTLD_NOW);
    entry_fn entry = h != NULL ? (entry_fn)dlsym(h, "sel_plugin_entry") : NULL;
    sel_library* lib;

    check(entry != NULL, "dlopen openmpt module");
    if (entry == NULL || mkdtemp(root) == NULL) return;
    g_reg = entry(1u);
    snprintf(db, sizeof db, "%s/library.db", root);
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, db);
    check(lib != NULL, "open real");
    if (lib != NULL) {
        sel_library_set_registry(lib, reg_count, reg_get);
        scan(lib, song);
        check(sel_library_hash_local(lib) >= 0, "idle pass real");
        sel_query_set_fts5(1);
        check(query_rows(lib, "text:sample") == 1, "openmpt: sample name word found through FTS5");
        sel_query_set_fts5(0);
        check(query_rows(lib, "text:\"test sample 1\"") == 1, "openmpt: sample name found through LIKE");
        check(query_rows(lib, "text:nothere") == 0, "openmpt: no hit");
        sel_query_set_fts5(1);
        sel_library_close(lib);
    }
    unlink(db);
    rmdir(root);
}

int main(int argc, char** argv) {
    char root[] = "/tmp/selecta_strings_XXXXXX";
    char db[320], with[320], without[320];
    sel_library* lib;
    sqlite3_stmt* st = NULL;
    int rows = -1;

    if (mkdtemp(root) == NULL) return 2;
    snprintf(db, sizeof db, "%s/library.db", root);
    snprintf(with, sizeof with, "%s/with.mod", root);
    snprintf(without, sizeof without, "%s/without.mod", root);
    put_file(with, "STRGgreetings to the scene");
    put_file(without, "plain bytes no replayer claims");
    g_hooks[0].tag = SEL_RP_PROBE;
    g_hooks[0].value = SEL_TAG_FN(probe);
    g_hooks[1].tag = SEL_RP_STRINGS;
    g_hooks[1].value = SEL_TAG_FN(strings);
    g_hooks[2].tag = SEL_TAG_END;
    g_desc[0].tag = SEL_PLUGIN_ID;
    g_desc[0].value = (uintptr_t) "fake_strings";
    g_desc[1].tag = SEL_PLUGIN_KIND;
    g_desc[1].value = SEL_PLUGIN_KIND_REPLAYER;
    g_desc[2].tag = SEL_PLUGIN_TAGS;
    g_desc[2].value = (uintptr_t)g_hooks;
    g_desc[3].tag = SEL_TAG_END;

    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, db);
    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    sel_library_set_registry(lib, reg_count, reg_get);
    scan(lib, with);
    scan(lib, without);
    check(sel_library_hash_local(lib) >= 0, "idle pass");
    if (sqlite3_prepare_v2(lib->db, "SELECT COUNT(*) FROM song_strings", -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        rows = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    check(rows == 2, "one song_strings row per song, empty for the unclaimed file");

    /* FTS5 path */
    sel_query_set_fts5(1);
    check(query_rows(lib, "text:acid_bass") == 1, "fts: sample name found");
    check(query_rows(lib, "text:greetings") == 1, "fts: song message word found");
    check(query_rows(lib, "text:fixt") == 1, "fts: token prefix found");
    check(query_rows(lib, "text:nothere") == 0, "fts: no hit");
    /* LIKE fallback path (FTS5 reported absent): substring match over the same table */
    sel_query_set_fts5(0);
    check(query_rows(lib, "text:acid_bass") == 1, "like: sample name found");
    check(query_rows(lib, "text:to the sc") == 1 || query_rows(lib, "text:\"to the sc\"") == 1, "like: substring found");
    check(query_rows(lib, "text:nothere") == 0, "like: no hit");
    check(query_rows(lib, "text:acid%") == 0, "like: percent is escaped, not a wildcard");
    sel_query_set_fts5(1);

    sel_library_close(lib);
    if (argc >= 3) real_module(argv[1], argv[2]);
    unlink(db);
    unlink(with);
    unlink(without);
    rmdir(root);
    if (!g_fail) printf("library_strings_ok\n");
    return g_fail;
}
