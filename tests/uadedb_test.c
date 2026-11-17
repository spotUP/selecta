/* I774k rows R1-R3: the derived uade-tools files. A small tools-tree fixture is baked by sel_uadedb_bake, then
 * read: songlengths v2 (ms, songend type, duplicate flag; a v1 file still answers), uadeinfo (format, channels,
 * authors, publisher, album, year), and the md5 bridge built from a source row (aminet.tsv). The md5 twin files
 * are cross-checked: one planted wrong twin row must show as exactly one mismatch. */
#include "songlen.h"
#include "uadedb.h"

#include <selecta/uimodel/siinfo.h>

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

static void put_file(const char* dir, const char* rel, const char* text) {
    char path[512];
    FILE* f;
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    f = fopen(path, "wb");
    if (f == NULL) {
        printf("cannot write %s\n", path);
        exit(2);
    }
    fputs(text, f);
    fclose(f);
}

static void make_dir(const char* dir, const char* rel) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    mkdir(path, 0777);
}

int main(void) {
    char root[] = "/tmp/selecta_uadedb_XXXXXX";
    char out[300], bin[320], cmd[600], rows[4096];
    sel_uadedb_stats st;
    sel_uadeinfo info;
    uint32_t ms = 0, end = 9, dup = 9, ea = 0, eb = 0, e = 0, i;
    FILE* f;
    size_t n = 0;

    if (mkdtemp(root) == NULL) return 2;
    snprintf(out, sizeof out, "%s/out", root);
    make_dir(root, "tsv");
    make_dir(root, "tsv/pretty");
    make_dir(root, "tsv/pretty/xxh32");
    make_dir(root, "tsv/pretty/md5");
    make_dir(root, "songdb");
    make_dir(root, "songdb/sources");
    make_dir(root, "songdb/sources/site");
    make_dir(root, "songdb/sources/cd");
    make_dir(root, "out");

    /* R1 rows: a dup flag on subsong 2 of A, songend p+s / l / p, plus filler keys for the binary search */
    n += (size_t)snprintf(rows + n, sizeof rows - n, "aaaaaaaa0100\t1\t1000,p+s 2000,l,!\nbbbbbbbb0200\t0\t3000,p\n");
    n += (size_t)snprintf(rows + n, sizeof rows - n, "cccccccc0300\t0\t4000,p+v\n");
    for (i = 0; i < 20; i++) n += (size_t)snprintf(rows + n, sizeof rows - n, "%02x00000000%02x\t0\t%u,s\n", i + 1, i, 100 + i);
    put_file(root, "tsv/pretty/xxh32/songlengths.tsv", rows);
    put_file(root, "tsv/pretty/xxh32/modinfos.tsv",
             "aaaaaaaa0100\tProtracker\t4\nbbbbbbbb0200\tImpulse Tracker 2.13\t15\ncccccccc0300\tFoo\t\n");
    put_file(root, "tsv/pretty/xxh32/metadata.tsv",
             "aaaaaaaa0100\tGlue Master~Kid\tMahoney\tMusicdisk 1\t1988\nbbbbbbbb0200\tXplore\t\t\t\n");
    /* md5 twins: A and B correct except B's album, which is planted wrong; one md5 no source row knows */
    put_file(root, "tsv/pretty/md5/modinfos.tsv", "111111111111\tProtracker\t4\n222222222222\tImpulse Tracker 2.13\t15\n444444444444\tX\t1\n");
    put_file(root, "tsv/pretty/md5/metadata.tsv",
             "111111111111\tGlue Master~Kid\tMahoney\tMusicdisk 1\t1988\n222222222222\tXplore\t\tWRONG\t\n");
    /* source rows: md5, subsong, ms, songend, player, format, channels, size (256 = 0x0100), xxh32, crc32, path */
    put_file(root, "songdb/sources/site/aminet.tsv",
             "11111111111111111111111111111111\t1\t1000\tp+s\tuade\tProtracker\t4\t256\taaaaaaaa\t00000000\tmods/a.lha/a\n"
             "11111111111111111111111111111111\t2\t2000\tl\tuade\tProtracker\t4\t256\taaaaaaaa\t00000000\tmods/a.lha/a\n"
             "22222222222222222222222222222222\t0\t3000\tp\tuade\tIT\t15\t512\tbbbbbbbb\t00000000\tmods/b\n"
             "55555555555555555555555555555555\t0\t1\tp\tuade\tX\t1\t1\tdddddddd\t00000000\tmods/c\n");

    /* R5: the same tune A in a second site file and a CD file; display names come from sources.md */
    put_file(root, "songdb/sources/site/other.tsv",
             "11111111111111111111111111111111\t1\t1000\tp+s\tuade\tProtracker\t4\t256\taaaaaaaa\t00000000\tx/a\n");
    put_file(root, "songdb/sources/cd/disc1.tsv",
             "11111111111111111111111111111111\t1\t1000\tp+s\tuade\tProtracker\t4\t256\taaaaaaaa\t00000000\ta\n");
    put_file(root, "sources.md", "### Sources\n\n#### Site\n\n- **Aminet** - https://aminet.net/\n\n#### CD / DVD\n\n- **Disc 1** - x\n");
    /* R4: A's AMP row equals its metadata (dropped); B has an AMP and an UnExoticA row (kept); one unknown md5 */
    put_file(root, "tsv/pretty/md5/amp.tsv",
             "111111111111\tGlue Master~Kid\tMahoney\tMusicdisk 1\t1988\n222222222222\tXplore Alt\t\t\t\n999999999999\tZ\t\t\t\n");
    put_file(root, "tsv/pretty/md5/unexotica.tsv", "222222222222\tXplore\tGame House\tGame Title\t1991\n");

    snprintf(cmd, sizeof cmd, "%s", out);
    CHECK(sel_uadedb_bake(root, out, &st) == 0);
    CHECK(st.songlen_entries == 23);
    CHECK(st.info_entries == 3); /* A, B, C have a format string */
    CHECK(st.md5_mapped == 2 && st.md5_unmapped == 1);
    CHECK(st.md5_mismatch == 1 && st.md5_twin_unmapped == 1 && st.md5_checked >= 4);

    /* R1: flags */
    snprintf(bin, sizeof bin, "%s/songlengths.bin", out);
    CHECK(sel_songlen_lookup_ex(bin, "aaaaaaaa0100", 1, &ms, &end, &dup, &ea) && ms == 1000u && dup == 0u);
    CHECK(strcmp(sel_songend_name(end), "p+s") == 0);
    CHECK(sel_songlen_lookup_ex(bin, "aaaaaaaa0100", 2, &ms, &end, &dup, NULL) && ms == 2000u && dup == 1u);
    CHECK(strcmp(sel_songend_name(end), "l") == 0);
    CHECK(sel_songlen_lookup(bin, "aaaaaaaa0100", 2, &ms, NULL) && ms == 2000u); /* old call: flags masked */
    CHECK(sel_songlen_lookup_ex(bin, "cccccccc0300", 0, &ms, &end, &dup, &e) && strcmp(sel_songend_name(end), "p+v") == 0);
    CHECK(sel_songlen_find(bin, "bbbbbbbb0200", &eb) && eb != ea);

    /* a version 1 file (no flag bits: the whole word is ms) still answers */
    snprintf(bin, sizeof bin, "%s/v1.bin", out);
    f = fopen(bin, "wb");
    if (f != NULL) {
        static const uint8_t v1[] = {'S', 'L', 'D', 'B', 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 28,
                                     0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0, 1, 0, 0, 0, 0,
                                     0x88, 0, 0, 1};
        fwrite(v1, 1, sizeof v1, f);
        fclose(f);
    }
    CHECK(sel_songlen_lookup_ex(bin, "aabbccddeeff", 0, &ms, &end, &dup, NULL) && ms == 0x88000001u && end == 0u && dup == 0u);

    /* R2: uadeinfo by entry */
    snprintf(bin, sizeof bin, "%s/%s", out, SEL_UADEINFO_NAME);
    CHECK(sel_uadeinfo_get(bin, ea, &info));
    CHECK(strcmp(info.format, "Protracker") == 0 && info.channels == 4u && strcmp(info.authors, "Glue Master~Kid") == 0);
    CHECK(strcmp(info.publishers, "Mahoney") == 0 && strcmp(info.album, "Musicdisk 1") == 0 && info.year == 1988u);
    CHECK(sel_uadeinfo_get(bin, eb, &info) && strcmp(info.format, "Impulse Tracker 2.13") == 0 && info.channels == 15u);
    CHECK(strcmp(info.authors, "Xplore") == 0 && info.album[0] == '\0' && info.year == 0u);
    CHECK(sel_uadeinfo_get(bin, e, &info) && strcmp(info.format, "Foo") == 0 && info.channels == 0u);
    CHECK(!sel_uadeinfo_get(bin, 9999u, &info));

    /* R3: md5 bridge */
    snprintf(bin, sizeof bin, "%s/%s", out, SEL_UADEMD5_NAME);
    CHECK(sel_uademd5_lookup(bin, "11111111111111111111111111111111", &e) && e == ea);
    CHECK(sel_uademd5_lookup(bin, "222222222222", &e) && e == eb);
    CHECK(!sel_uademd5_lookup(bin, "55555555555555555555555555555555", &e)); /* key not in songlengths */
    CHECK(!sel_uademd5_lookup(bin, "999999999999", &e));

    /* R4: per-source credit rows of B (entry eb); A's only row was dropped */
    {
        sel_uadecredit cr[4];
        uint16_t ids[8];
        int kind = -1;
        char file[32], disp[64];
        snprintf(bin, sizeof bin, "%s/%s", out, SEL_UADECREDITS_NAME);
        CHECK(st.credit_kept[0] == 1 && st.credit_dropped[0] == 1 && st.credit_unmapped[0] == 1);
        CHECK(st.credit_kept[4] == 1 && st.credit_rows == 2);
        CHECK(sel_uadecredits_get(bin, eb, cr, 4) == 2);
        CHECK(strcmp(sel_uadecredit_source_name(cr[0].source), "AMP") == 0 && strcmp(cr[0].authors, "Xplore Alt") == 0);
        CHECK(strcmp(sel_uadecredit_source_name(cr[1].source), "UnExoticA") == 0 && strcmp(cr[1].publishers, "Game House") == 0 &&
              strcmp(cr[1].album, "Game Title") == 0 && cr[1].year == 1991u);
        CHECK(sel_uadecredits_get(bin, ea, cr, 4) == 0);
        CHECK(sel_uadecredits_get(bin, 9999u, cr, 4) == 0);

        /* R5: A is in aminet + other (site) and disc1 (cd); B only in aminet */
        snprintf(bin, sizeof bin, "%s/%s", out, SEL_UADEPROV_NAME);
        CHECK(sel_uadeprov_get(bin, ea, ids, 8) == 3 && ids[0] == 0 && ids[1] == 1 && ids[2] == 2);
        CHECK(sel_uadeprov_source(bin, ids[0], &kind, file, sizeof file, disp, sizeof disp) && kind == SEL_UPRV_SITE &&
              strcmp(file, "aminet") == 0 && strcmp(disp, "Aminet") == 0);
        CHECK(sel_uadeprov_source(bin, ids[1], &kind, file, sizeof file, disp, sizeof disp) && kind == SEL_UPRV_SITE &&
              strcmp(disp, "other") == 0);
        CHECK(sel_uadeprov_source(bin, ids[2], &kind, file, sizeof file, disp, sizeof disp) && kind == SEL_UPRV_CD &&
              strcmp(disp, "Disc 1") == 0);
        CHECK(sel_uadeprov_get(bin, eb, ids, 8) == 1 && ids[0] == 0);
        CHECK(!sel_uadeprov_source(bin, 3u, &kind, file, sizeof file, disp, sizeof disp));
        CHECK(st.prov_sources[SEL_UPRV_SITE] == 2 && st.prov_sources[SEL_UPRV_CD] == 1 && st.prov_pairs == 4);
    }

    /* I774n R8: the song info section lines */
    {
        static sel_uim_si_input in;
        sel_uim_si_line ln[SEL_UIM_SI_LINES_MAX];
        uint32_t c;
        CHECK(sel_uadesong_load(out, "aaaaaaaa0100", &in));
        c = sel_uim_si_lines(&in, ln, SEL_UIM_SI_LINES_MAX);
        CHECK(c == 13u); /* Credits heading + 6; Found In heading, Sites 2 + 2 names, CD-ROMs 1 + 1 name */
        CHECK(ln[0].kind == SEL_UIM_SI_HEADING && strcmp(ln[0].key, "Credits") == 0);
        CHECK(strcmp(ln[1].key, "Authors") == 0 && strcmp(ln[1].val, "Glue Master, Kid") == 0);
        CHECK(strcmp(ln[2].key, "Group / Publisher") == 0 && strcmp(ln[2].val, "Mahoney") == 0);
        CHECK(strcmp(ln[3].val, "Musicdisk 1") == 0 && strcmp(ln[4].key, "Year") == 0 && strcmp(ln[4].val, "1988") == 0);
        CHECK(strcmp(ln[5].val, "Protracker") == 0 && strcmp(ln[6].key, "Channels") == 0 && strcmp(ln[6].val, "4") == 0);
        CHECK(ln[7].kind == SEL_UIM_SI_HEADING && strcmp(ln[7].key, "Found In") == 0);
        CHECK(ln[8].kind == SEL_UIM_SI_FOUND_KIND && strcmp(ln[8].key, "Sites") == 0 && strcmp(ln[8].val, "2") == 0);
        CHECK(ln[9].kind == SEL_UIM_SI_FOUND_NAME && strcmp(ln[9].val, "Aminet") == 0 && strcmp(ln[10].val, "other") == 0);
        CHECK(ln[11].kind == SEL_UIM_SI_FOUND_KIND && strcmp(ln[11].key, "CD-ROMs") == 0 && strcmp(ln[11].val, "1") == 0);
        CHECK(strcmp(ln[12].val, "Disc 1") == 0);

        /* B: only a one-site provenance and two other sources; no Same Tune / Demozoo section anywhere */
        CHECK(sel_uadesong_load(out, "bbbbbbbb0200", &in));
        c = sel_uim_si_lines(&in, ln, SEL_UIM_SI_LINES_MAX);
        CHECK(strcmp(ln[0].key, "Credits") == 0 && strcmp(ln[2].val, "Impulse Tracker 2.13") == 0 && strcmp(ln[3].val, "15") == 0);
        CHECK(strcmp(ln[4].key, "Other Sources") == 0);
        CHECK(ln[5].kind == SEL_UIM_SI_OTHER && strcmp(ln[5].key, "AMP") == 0 && strcmp(ln[5].val, "Xplore Alt") == 0);
        CHECK(strcmp(ln[6].key, "UnExoticA") == 0 && strcmp(ln[6].val, "Xplore, Game House, Game Title, 1991") == 0);
        CHECK(strcmp(ln[7].key, "Found In") == 0 && strcmp(ln[8].key, "Sites") == 0 && strcmp(ln[8].val, "1") == 0 && c == 10u);

        /* key not in the bake: no input, no lines */
        CHECK(!sel_uadesong_load(out, "ffffffffffff", &in));
        CHECK(sel_uim_si_lines(&in, ln, SEL_UIM_SI_LINES_MAX) == 0u);
    }

    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", root);
    (void)system(cmd);
    if (g_fail) {
        printf("uadedb_test: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("uadedb_ok\n");
    return 0;
}
