/* Host tool (I605, I774k): songlen_bake <songlengths.tsv> <out.bin> bakes the audacious-uade-tools song length
 * database into the sorted binary file src/library/songlen.c reads by seeks (prints "songlen_entries=N
 * songlen_bytes=B"). With a directory as first argument (uadedb_bake <tools dir> <out dir>) it bakes every derived
 * file (songlengths.bin, uadeinfo.bin, uademd5.bin) from an audacious-uade-tools tree. */
#include "songlen.h"
#include "uadedb.h"

#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
    long bytes = 0;
    long n;
    struct stat sb;
    if (argc != 3) {
        fprintf(stderr, "usage: songlen_bake <songlengths.tsv> <out.bin> | uadedb_bake <tools dir> <out dir>\n");
        return 2;
    }
    if (stat(argv[1], &sb) == 0 && S_ISDIR(sb.st_mode)) {
        sel_uadedb_stats s;
        if (sel_uadedb_bake(argv[1], argv[2], &s) != 0) {
            fprintf(stderr, "uadedb_bake: failed on %s\n", argv[1]);
            return 1;
        }
        printf("songlen_entries=%ld songlen_bytes=%ld\n", s.songlen_entries, s.songlen_bytes);
        printf("info_entries=%ld pool_bytes=%ld info_bytes=%ld info_unmapped=%ld\n", s.info_entries, s.pool_bytes,
               s.info_bytes, s.info_unmapped);
        printf("md5_checked=%ld md5_mismatch=%ld md5_whitelisted=%ld md5_twin_unmapped=%ld\n", s.md5_checked,
               s.md5_mismatch, s.md5_whitelisted, s.md5_twin_unmapped);
        printf("md5_mapped=%ld md5_unmapped=%ld md5_conflict=%ld md5_bytes=%ld\n", s.md5_mapped, s.md5_unmapped,
               s.md5_conflict, s.md5_bytes);
        return 0;
    }
    n = sel_songlen_bake(argv[1], argv[2], &bytes);
    if (n < 0) {
        fprintf(stderr, "songlen_bake: failed on %s\n", argv[1]);
        return 1;
    }
    printf("songlen_entries=%ld songlen_bytes=%ld\n", n, bytes);
    return 0;
}
