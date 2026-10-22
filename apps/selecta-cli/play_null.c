#include "cli.h"

#include <stdio.h>

int sel_cli_play_null(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fputs("play-null: not implemented\n", stderr);
    return 1;
}
