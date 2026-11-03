/* I899a: the shared entropy pool (ready threshold, distinct outputs, seed file) and the cast HTTP server refusing to
 * start, with no guessable token, while the pool is not ready. argv[1]: a scratch directory for the seed file. */
#include "cast/cast_http.h"
#include "entropy.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            printf("FAIL line %d: %s\n", __LINE__, #c);           \
            fails++;                                              \
        }                                                         \
    } while (0)

int main(int argc, char** argv) {
    unsigned char a[16], b[16], zero[16] = {0};
    char seedpath[512];
    sel_cast_http srv;
    unsigned i;

    if (argc < 2) {
        return 2;
    }
    /* Pool backend: not ready until the threshold, then distinct outputs. */
    sel_entropy_test_mode(2);
    CHECK(!sel_entropy_ready());
    for (i = 0; i < SEL_ENTROPY_THRESHOLD - 1; i++) {
        sel_entropy_add(i * 2654435761u);
    }
    CHECK(!sel_entropy_ready());
    CHECK(sel_entropy_bytes(a, sizeof a) == -1);
    sel_entropy_add(12345u);
    CHECK(sel_entropy_ready());
    CHECK(sel_entropy_samples() == SEL_ENTROPY_THRESHOLD);
    CHECK(sel_entropy_bytes(a, sizeof a) == 0);
    CHECK(sel_entropy_bytes(b, sizeof b) == 0);
    CHECK(memcmp(a, b, sizeof a) != 0);
    CHECK(memcmp(a, zero, sizeof a) != 0);

    /* Seed file: written from output (not the raw pool), read back, never makes an empty pool ready. */
    snprintf(seedpath, sizeof seedpath, "%s/selecta.seed", argv[1]);
    remove(seedpath);
    CHECK(sel_entropy_seed_load(seedpath) == -1); /* absent, path remembered */
    CHECK(sel_entropy_seed_save() == 0);
    CHECK(sel_entropy_seed_load(seedpath) == 0);
    sel_entropy_test_mode(2);
    CHECK(sel_entropy_seed_load(seedpath) == 0);
    CHECK(!sel_entropy_ready());

    /* Desktop backend: urandom, ready at once, distinct. */
    sel_entropy_test_mode(0);
    CHECK(sel_entropy_ready());
    CHECK(sel_entropy_bytes(a, sizeof a) == 0 && sel_entropy_bytes(b, sizeof b) == 0 && memcmp(a, b, sizeof a) != 0);

    /* Cast HTTP: refuses to open without a ready pool, serves a 32-hex token with one. */
    sel_entropy_test_mode(1);
    CHECK(sel_cast_http_open(&srv, argv[0], "audio/mpeg") == SEL_CAST_HTTP_E_ENTROPY);
    CHECK(srv.lh == -1 && srv.token[0] == '\0');
    sel_entropy_test_mode(0);
    CHECK(sel_cast_http_open(&srv, argv[0], "audio/mpeg") == 0);
    CHECK(strlen(srv.token) == SEL_CAST_HTTP_TOKEN_LEN);
    sel_cast_http_close(&srv);

    printf(fails == 0 ? "entropy_ok\n" : "entropy_failed\n");
    return fails != 0;
}
