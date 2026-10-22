/* selecta-cli: the headless entry point and the product's test surface.
 * Every M0a test drives this binary (plan 2026-10-22-m0-skeleton.md, T1 to T9). */
#include <selecta/api.h>

#include <stdio.h>
#include <string.h>

static int cmd_version(void) {
    printf("selecta-cli %s\n", sel_version_string());
    return 0;
}

static int usage(FILE* to) {
    fputs("usage: selecta-cli <subcommand>\n"
          "  version                 print version and API number\n",
          to);
    return to == stdout ? 0 : 2;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return usage(stderr);
    }
    if (strcmp(argv[1], "version") == 0) {
        return cmd_version();
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        return usage(stdout);
    }
    fprintf(stderr, "selecta-cli: unknown subcommand '%s'\n", argv[1]);
    return usage(stderr);
}
