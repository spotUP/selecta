/* `selecta-cli devilbox login <user> [--password-stdin] [--config-dir <dir>]` and the @ratings_read script
 * helper (I173, D-M3-12). The password is read from standard input, one line, never from argv. The token goes
 * to <config>/devilbox.token (mode 0600) through the library's DEViLBOX client. */
#include "cli.h"
#include "db.h"
#include "net/devilbox.h"

#include <selecta/engine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__amigaos__) && !defined(AMIGA)
#include <termios.h>
#include <unistd.h>
#endif

/* Reads the password line from stdin; no echo when stdin is a terminal. */
static int read_password(char* out, size_t cap) {
#if !defined(__amigaos__) && !defined(AMIGA)
    struct termios old, quiet;
    int tty = isatty(0) && tcgetattr(0, &old) == 0;

    if (tty) {
        quiet = old;
        quiet.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(0, TCSANOW, &quiet);
        fprintf(stderr, "Password: ");
    }
#endif
    if (fgets(out, (int)cap, stdin) == NULL) out[0] = '\0';
    out[strcspn(out, "\r\n")] = '\0';
#if !defined(__amigaos__) && !defined(AMIGA)
    if (tty) {
        tcsetattr(0, TCSANOW, &old);
        fprintf(stderr, "\n");
    }
#endif
    return out[0] != '\0';
}

int sel_cli_devilbox(int argc, char** argv) {
    char dir[256];
    char password[256];
    const char* user;
    const char* config = NULL;
    int status = 0;
    int ok;
    int i;

    if (argc < 2 || strcmp(argv[0], "login") != 0) {
        fprintf(stderr, "usage: selecta-cli devilbox login <user> [--password-stdin] [--config-dir <dir>]\n");
        return 2;
    }
    user = argv[1];
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--config-dir") == 0 && i + 1 < argc) {
            config = argv[++i];
        } else if (strcmp(argv[i], "--password-stdin") != 0) {
            fprintf(stderr, "selecta-cli: devilbox login takes no argument %s (the password comes from stdin)\n", argv[i]);
            return 2;
        }
    }
    if (config == NULL) {
        if (!sel_config_dir(dir, sizeof dir)) {
            fprintf(stderr, "selecta-cli: no config directory\n");
            return 1;
        }
        config = dir;
    }
    if (!read_password(password, sizeof password)) {
        fprintf(stderr, "selecta-cli: no password on stdin\n");
        return 2;
    }
    (void)sel_config_mkdir(config);
    sel_devilbox_set_config_dir(config);
    ok = sel_devilbox_login(user, password, &status);
    memset(password, 0, sizeof password);
    printf("devilbox_login=%s status=%d\n", ok ? "ok" : "failed", status);
    return ok ? 0 : 1;
}

typedef struct read_ctx {
    sel_library* lib;
    const char* source;
    const char* const* paths;
    unsigned stored;
} read_ctx;

static void on_rating(void* ctx, unsigned index, int rating) {
    read_ctx* c = ctx;
    char key[600];

    snprintf(key, sizeof key, "%s:%s", c->source, c->paths[index]);
    if (sel_library_rating_set(c->lib, key, rating, SEL_RATING_SERVER)) {
        c->stored++;
        printf("RATING_READ key=%s stars=%d\n", key, rating);
    }
}

/* "@ratings_read <source> <n> [<path>...]": reads the ratings of n paths (the named ones first, then generated
 * `bulk/NNNNN.mod` paths) in batches and stores the user's own ratings in the library. */
void sel_cli_ratings_read(sel_library* lib, const char* args) {
    char source[16];
    char text[1024];
    char* named[16];
    const char** paths;
    char (*gen)[24];
    unsigned n = 0, nn = 0, i, requests;
    read_ctx ctx;
    char* tok;

    if (lib == NULL || sscanf(args, "%15s %u", source, &n) != 2 || n == 0 || n > 100000u) return;
    snprintf(text, sizeof text, "%s", args);
    (void)strtok(text, " ");
    (void)strtok(NULL, " ");
    while ((tok = strtok(NULL, " ")) != NULL && nn < 16u) named[nn++] = tok;
    paths = malloc(n * sizeof *paths);
    gen = malloc(n * sizeof *gen);
    if (paths == NULL || gen == NULL) {
        free(paths);
        free(gen);
        return;
    }
    for (i = 0; i < n; i++) {
        if (i < nn) {
            paths[i] = named[i];
        } else {
            snprintf(gen[i], sizeof gen[i], "bulk/%05u.mod", i);
            paths[i] = gen[i];
        }
    }
    ctx.lib = lib;
    ctx.source = source;
    ctx.paths = paths;
    ctx.stored = 0;
    requests = sel_devilbox_ratings_read(source, paths, n, on_rating, &ctx);
    printf("RATINGS_READ keys=%u requests=%u stored=%u\n", n, requests, ctx.stored);
    free(paths);
    free(gen);
}
