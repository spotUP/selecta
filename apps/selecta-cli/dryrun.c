/* `selecta-cli --dry-run [--tooltypes <file>] [KEY=VALUE ...]` (T58): runs the
 * start options through the one parser (src/engine/options.c) and prints the
 * result without starting anything. ToolTypes arrive as a text file of
 * KEY=VALUE lines, the same words the command line takes. */
#include "cli.h"
#include "../../src/engine/options.h"
#include "../../src/core/pal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    char* buf;
    long n;

    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = n >= 0 ? malloc((size_t)n + 1u) : NULL;
    if (buf != NULL) {
        size_t got = fread(buf, 1, (size_t)n, f);
        buf[got] = '\0';
    }
    fclose(f);
    return buf;
}

int sel_cli_dry_run(int argc, char** argv) {
    sel_prefs prefs;
    sel_opts opts;
    int i, r = SEL_OPTS_OK;

    sel_prefs_init(&prefs);
    memset(&opts, 0, sizeof opts);
    for (i = 0; i < argc && r == SEL_OPTS_OK; i++) {
        if (strcmp(argv[i], "--tooltypes") == 0 && i + 1 < argc) {
            char* text = read_file(argv[++i]);
            if (text == NULL) {
                fprintf(stderr, "selecta-cli: cannot read %s\n", argv[i]);
                return 1;
            }
            r = sel_opts_parse_text(&prefs, &opts, text);
            free(text);
        } else {
            r = sel_opts_apply_word(&prefs, &opts, argv[i]);
        }
    }
    if (r != SEL_OPTS_OK) {
        fprintf(stderr, "selecta-cli: bad option (%s)\n", r == SEL_OPTS_UNKNOWN_KEY ? "unknown key" : "bad value");
        return 2;
    }
    printf("opts=%u\n", (unsigned)opts.count);
    printf("prefs_hash=%08x\n", (unsigned)sel_opts_hash(&prefs));
    printf("output=%s\n", prefs.value[SEL_PREF_OUTPUT_ID]);
    printf("buffer=%s\n", prefs.value[SEL_PREF_OUTPUT_BUFFER]);
    printf("led=%s\n", prefs.value[SEL_PREF_OUTPUT_LED]);
    /* I117: apply the option to this task and print what the OS reports back */
    printf("task_pri=%d\n", sel_pal_set_task_priority(sel_opts_priority(&prefs)));
    return 0;
}
