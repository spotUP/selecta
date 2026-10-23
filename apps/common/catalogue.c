#include "catalogue.h"

#include <selecta/machine.h>

#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Opens an in-memory library and imports the TSV catalogue `tsv` (key, source, path, name, artist,
 * format, year per line). NULL, with a message on stderr, when it cannot. Used by `library search`
 * and `script --catalogue`. */
sel_library* sel_cli_open_catalogue(const char* tsv) {
    const sel_host* host = sel_host_default();
    sel_library* lib;
    sel_lib_job job;
    FILE* f = fopen(tsv, "rb");
    char* buf;
    long n;

    if (f == NULL) {
        fprintf(stderr, "selecta-cli: cannot read catalogue %s\n", tsv);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1u);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[n] = '\0';
    lib = sel_library_open(host, sel_machine_probe(host), 0, ":memory:");
    if (lib == NULL) {
        fputs("selecta-cli: library open failed\n", stderr);
        free(buf);
        return NULL;
    }
    memset(&job, 0, sizeof job);
    job.text = buf;
    if (!sel_library_run(lib, SEL_LIB_REQ_IMPORT, &job) || job.error[0] != '\0') {
        fputs("selecta-cli: catalogue import failed\n", stderr);
        sel_library_close(lib);
        lib = NULL;
    }
    free(buf);
    return lib;
}
