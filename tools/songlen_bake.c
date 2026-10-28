/* Host tool (I605): songlen_bake <songlengths.tsv> <out.bin>. Bakes the audacious-uade-tools song length database
 * into the sorted binary file src/library/songlen.c reads by seeks. Prints "songlen_entries=N songlen_bytes=B". */
#include "songlen.h"

#include <stdio.h>

int main(int argc, char** argv) {
    long bytes = 0;
    long n;
    if (argc != 3) {
        fprintf(stderr, "usage: songlen_bake <songlengths.tsv> <out.bin>\n");
        return 2;
    }
    n = sel_songlen_bake(argv[1], argv[2], &bytes);
    if (n < 0) {
        fprintf(stderr, "songlen_bake: failed on %s\n", argv[1]);
        return 1;
    }
    printf("songlen_entries=%ld songlen_bytes=%ld\n", n, bytes);
    return 0;
}
