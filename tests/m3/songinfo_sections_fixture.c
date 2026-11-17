/* I774n R10: bakes a one-song uade-tools fixture for the text UI song info sections test. Usage: fixture <song> <outdir> [full]. full adds other sources and provenance (Found In) rows.
 * The key comes from sel_uade_key_file, so the bake row matches the real file the selecta binary opens. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    mk(root, "songdb/sources/cd");
    snprintf(row, sizeof row, "%s\t0\t60000,s\n", key);
    put(root, "tsv/pretty/xxh32/songlengths.tsv", row);
    snprintf(row, sizeof row, "%s\tProtracker\t4\n", key);
    put(root, "tsv/pretty/xxh32/modinfos.tsv", row);
    snprintf(row, sizeof row, "%s\tFixture Title\tFixture Author\tFixture Album\t1991\n", key);
    put(root, "tsv/pretty/xxh32/metadata.tsv", row);
    put(root, "tsv/pretty/md5/modinfos.tsv", ""); /* the bake needs both passes present */
    put(root, "tsv/pretty/md5/metadata.tsv", "");
    put(root, "sources.md", "### Sources\n\n#### Site\n\n- **Aminet** - https://aminet.net/\n");
    if (argc > 3) {
        /* source rows: md5, subsong, ms, songend, player, format, channels, size, xxh32, crc32, path; the md5 is made up, the xxh32 is the key's */
        char xx[9], src[800];
        unsigned size = (unsigned)strtoul(key + 8, NULL, 16);
        memcpy(xx, key, 8);
        xx[8] = '\0';
        snprintf(src, sizeof src, "11111111111111111111111111111111\t0\t60000\ts\tuade\tProtracker\t4\t%u\t%s\t00000000\tmods/a\n", size, xx);
        put(root, "songdb/sources/site/aminet.tsv", src);
        put(root, "songdb/sources/site/other.tsv", src);
        put(root, "songdb/sources/cd/disc1.tsv", src);
        put(root, "sources.md", "### Sources\n\n#### Site\n\n- **Aminet** - https://aminet.net/\n- **Other Site** - x\n\n#### CD / DVD\n\n- **Disc 1** - x\n");
        put(root, "tsv/pretty/md5/amp.tsv", "111111111111\tAmp Author\tAmp Publisher\tAmp Album\t1992\n");
        put(root, "tsv/pretty/md5/unexotica.tsv", "111111111111\tUnex Author\tGame House\tGame Title\t1993\n");
    }
    if (sel_uadedb_bake(root, out, &st) != 0) {
        printf("bake failed\n");
        return 1;
    }
    printf("fixture_ok %s\n", key);
    return 0;
}
