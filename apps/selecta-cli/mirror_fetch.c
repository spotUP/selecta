/* `selecta-cli library fetch <source>:<path> [--machine <name>] [--rounds n] [--out file]` (T69, I156):
 * fetch one file through the Mirrors transport. --machine with a fake Amiga machine selects the
 * DEViLBOX route. --rounds repeats the fetch in one process so the ranking learned in round 1
 * shows in round 2. Prints per-round `mirror_round_<n>_tries= _retries= _served=` lines and the
 * totals of the last round as `mirror_tries= mirror_retries= mirror_served= mirror_order=`. */
#include <selecta/machine.h>

#include "cli.h"
#include "net/mirrors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int on_body(void* ctx, const void* data, size_t len) {
    FILE* f = ctx;
    return f == NULL || fwrite(data, 1, len, f) == len;
}

static void on_restart(void* ctx) {
    FILE* f = ctx;
    if (f != NULL) {
        rewind(f);
    }
}

int sel_cli_library_fetch(int argc, char** argv) {
    const char* spec = NULL;
    const char* out_path = NULL;
    int amiga = 0;
    unsigned rounds = 1, r, k;
    char source[24];
    const char* colon;
    sel_mirror_set set;
    sel_mirror_res res;
    FILE* out = NULL;
    int ok = 0, i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--machine") == 0 && i + 1 < argc) {
            amiga = sel_machine_fake(argv[++i]) != NULL;
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = (unsigned)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (spec == NULL && argv[i][0] != '-') {
            spec = argv[i];
        } else {
            spec = NULL;
            break;
        }
    }
    colon = spec != NULL ? strchr(spec, ':') : NULL;
    if (colon == NULL || colon == spec || (size_t)(colon - spec) >= sizeof source || rounds == 0) {
        fputs("usage: selecta-cli library fetch <source>:<path> [--machine <name>] [--rounds n] [--out file]\n",
              stderr);
        return 2;
    }
    memcpy(source, spec, (size_t)(colon - spec));
    source[colon - spec] = '\0';
    if (sel_mirrors_init(&set, source, amiga) == 0) {
        fprintf(stderr, "selecta-cli: library fetch: no mirrors for source %s\n", source);
        return 2;
    }
    if (out_path != NULL) {
        out = fopen(out_path, "wb");
        if (out == NULL) {
            fprintf(stderr, "selecta-cli: library fetch: cannot write %s\n", out_path);
            return 2;
        }
    }
    for (r = 1; r <= rounds; r++) {
        if (out != NULL) {
            rewind(out);
        }
        ok = sel_mirrors_fetch(&set, colon + 1, on_body, on_restart, out, &res);
        printf("mirror_round_%u_tries=%u\nmirror_round_%u_retries=%u\nmirror_round_%u_served=%d\n", r, res.tries, r,
               res.retries, r, res.served);
    }
    if (out != NULL) {
        fclose(out);
    }
    printf("mirror_tries=%u\nmirror_retries=%u\nmirror_served=%d\nmirror_url=%s\nmirror_home=%u\nbody_bytes=%lu\n"
           "mirror_ok=%d\nmirror_order=",
           res.tries, res.retries, res.served, res.url, set.home, res.body_bytes, ok);
    for (k = 0; k < set.count; k++) {
        printf("%s%u", k > 0 ? "," : "", res.order[k]);
    }
    printf("\n");
    for (k = 0; k < set.count; k++) {
        printf("mirror_%u_base=%s\nmirror_%u_ttfb_ms=%u\nmirror_%u_failures=%u\n", k, set.base[k], k, set.ttfb_ms[k], k,
               set.failures[k]);
    }
    return ok ? 0 : 1;
}
