/* Host tool (I625a8e): library_template_bake <out.db>. Writes the empty library database a first start would build with
 * sel_schema_migrate (the 65 statements of SCHEMA_FINAL, user_version SEL_LIB_SCHEMA_VERSION), so sel_library_open can copy the
 * file instead of running the DDL, which costs 3336 ms on a 68020 (I625a8d). A SQLite database file is platform independent, so
 * the file made on the host is valid on the Amiga. Same code path as the product: this tool calls sel_schema_migrate itself. */
#include "db.h"
#include "schema.h"

#include <selecta/host.h>
#include <sqlite3.h>
#include <stdio.h>

int main(int argc, char** argv) {
    sqlite3* db = NULL;
    int ok;

    if (argc != 2) {
        fprintf(stderr, "usage: library_template_bake <out.db>\n");
        return 2;
    }
    remove(argv[1]);
    if (sqlite3_open(argv[1], &db) != SQLITE_OK) {
        fprintf(stderr, "library_template_bake: cannot create %s\n", argv[1]);
        return 1;
    }
    ok = sel_schema_migrate(db, sel_host_default()) && sel_schema_version(db) == SEL_LIB_SCHEMA_VERSION;
    sqlite3_close(db);
    if (!ok) {
        fprintf(stderr, "library_template_bake: migration failed\n");
        remove(argv[1]);
        return 1;
    }
    puts("library_template_ok");
    return 0;
}
