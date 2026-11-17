/* I774n R10: bakes a one-song uade-tools fixture for the text UI song info sections test. Usage: fixture <song> <outdir>.
 * The key comes from sel_uade_key_file, so the bake row matches the real file the selecta binary opens. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "uadedb.h"
#include "xxh32.h"

static void put(const char* dir, const char* rel, const char* text) {
    char p[600];
    FILE* f;
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    f = fopen(p, "wb");
    if (f == NULL) {
        printf("cannot write %s\n", p);
        exit(2);
    }
    fputs(text, f);
    fclose(f);
}

static void mk(const char* root, const char* rel) {
    char p[600];
    snprintf(p, sizeof p, "%s/%s", root, rel);
    mkdir(p, 0777);
}

int main(int argc, char** argv) {
    char key[SEL_UADE_KEY_CHARS + 1u], row[600];
    sel_uadedb_stats st;
    char root[600], out[600];
    if (argc < 3 || !sel_uade_key_file(argv[1], key)) {
        printf("usage: fixture <song> <outdir> (song unreadable?)\n");
        return 2;
    }
    snprintf(root, sizeof root, "%s/tools", argv[2]);
    snprintf(out, sizeof out, "%s/out", argv[2]);
    mkdir(argv[2], 0777);
    mkdir(root, 0777);
    mkdir(out, 0777);
    mk(root, "tsv");
    mk(root, "tsv/pretty");
    mk(root, "tsv/pretty/xxh32");
    mk(root, "tsv/pretty/md5");
    mk(root, "songdb");
    mk(root, "songdb/sources");
    mk(root, "songdb/sources/site");
    snprintf(row, sizeof row, "%s\t0\t60000,s\n", key);
    put(root, "tsv/pretty/xxh32/songlengths.tsv", row);
    snprintf(row, sizeof row, "%s\tProtracker\t4\n", key);
    put(root, "tsv/pretty/xxh32/modinfos.tsv", row);
    snprintf(row, sizeof row, "%s\tFixture Title\tFixture Author\tFixture Album\t1991\n", key);
    put(root, "tsv/pretty/xxh32/metadata.tsv", row);
    put(root, "tsv/pretty/md5/modinfos.tsv", ""); /* the bake needs both passes present */
    put(root, "tsv/pretty/md5/metadata.tsv", "");
    put(root, "sources.md", "### Sources\n\n#### Site\n\n- **Aminet** - https://aminet.net/\n");
    if (sel_uadedb_bake(root, out, &st) != 0) {
        printf("bake failed\n");
        return 1;
    }
    printf("fixture_ok %s\n", key);
    return 0;
}
