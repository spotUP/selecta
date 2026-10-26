/* I420: sel_devilbox_base defaults to https on TLS builds (this test is only built where TLS is compiled in),
 * and SELECTA_DEVILBOX_URL still overrides it. Every DEViLBOX request URL (login, ratings, votes, loudness,
 * lyrics, genre, songdb) is built from this one function. */
#include "net/mirrors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int bad = 0;

    unsetenv("SELECTA_DEVILBOX_URL");
    if (strcmp(sel_devilbox_base(), "https://devilbox.uprough.net") != 0) {
        printf("FAIL default base is %s\n", sel_devilbox_base());
        bad = 1;
    }
    setenv("SELECTA_DEVILBOX_URL", "http://127.0.0.1:1", 1);
    if (strcmp(sel_devilbox_base(), "http://127.0.0.1:1") != 0) {
        printf("FAIL override ignored: %s\n", sel_devilbox_base());
        bad = 1;
    }
    if (!bad) puts("devilbox_default_https_ok");
    return bad;
}
