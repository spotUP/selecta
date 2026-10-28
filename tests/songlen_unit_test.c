/* I605: the song length database tool and lookup. argv[1] = the songlen_bake executable. A tiny TSV fixture is baked
 * by the real tool, then looked up: key hit, miss, subsong numbering, unknown length, missing file, and the read
 * count of the binary search (it must stay logarithmic: the Amiga never loads the 470k-row file). */
#include "songlen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

int main(int argc, char** argv) {
    char dir[] = "/tmp/selecta_songlen_XXXXXX";
    char tsv[256], bin[256], cmd[1024];
    FILE* f;
    uint32_t ms = 0, reads = 0;
    long n, bytes = 0, i;

    if (argc != 2 || mkdtemp(dir) == NULL) return 2;
    snprintf(tsv, sizeof tsv, "%s/in.tsv", dir);
    snprintf(bin, sizeof bin, "%s/out.bin", dir);
    f = fopen(tsv, "wb");
    if (f == NULL) return 2;
    /* Unsorted on purpose; a duplicate key (the first row wins); a subsong list with a zero length. */
    fputs("ff5c7b3227e0\t0\t65920,p 65920,p,!\n", f);
    fputs("0000aa11bb22\t1\t250840,p+s\n", f);
    fputs("fffdc1d765c3\t0\t40880,l 117860,l 8780,s 0,s\n", f);
    fputs("0000aa11bb22\t0\t999,p\n", f);
    for (i = 0; i < 2000; i++) fprintf(f, "%02x%02x%08lx\t0\t%ld,p\n", (int)(i & 0xff), (int)(i >> 8) + 1, (unsigned long)i * 7919ul, 1000 + i);
    fclose(f);

    snprintf(cmd, sizeof cmd, "\"%s\" \"%s\" \"%s\" > \"%s/tool.out\"", argv[1], tsv, bin, dir);
    CHECK(system(cmd) == 0);
    CHECK(sel_songlen_lookup(bin, "fffdc1d765c3", 0, &ms, &reads) && ms == 40880u);
    CHECK(reads <= 14u); /* header + log2(2003) probes + the length */
    CHECK(sel_songlen_lookup(bin, "fffdc1d765c3", 2, &ms, NULL) && ms == 8780u);
    CHECK(!sel_songlen_lookup(bin, "fffdc1d765c3", 3, &ms, NULL)); /* length 0 = unknown */
    CHECK(!sel_songlen_lookup(bin, "fffdc1d765c3", 4, &ms, NULL)); /* past the subsongs */
    CHECK(sel_songlen_lookup(bin, "ff5c7b3227e0", 1, &ms, NULL) && ms == 65920u);
    CHECK(sel_songlen_lookup(bin, "0000aa11bb22", 1, &ms, NULL) && ms == 250840u);
    CHECK(!sel_songlen_lookup(bin, "0000aa11bb22", 0, &ms, NULL)); /* below the first subsong */
    CHECK(!sel_songlen_lookup(bin, "123456789abc", 0, &ms, NULL)); /* key miss */
    CHECK(!sel_songlen_lookup(bin, "not-a-hex-key", 0, &ms, NULL));
    CHECK(!sel_songlen_lookup("/nonexistent/songlengths.bin", "fffdc1d765c3", 0, &ms, NULL));
    n = sel_songlen_bake(tsv, bin, &bytes);
    CHECK(n == 2003); /* 2004 rows, one duplicate key dropped */
    CHECK(bytes > 16 + 2003 * 12);

    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("songlen_unit: %d failures\n", g_fail);
        return 1;
    }
    printf("songlen_unit_ok\n");
    return 0;
}
