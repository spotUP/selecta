/* I419: sel_dvb_lyrics_get maps a 404 to UNKNOWN whatever the body. The real server answers
 * 404 {"error":"unknown md5"} (DEViLBOX server/src/routes/lyrics.ts); the fixture also answers a plain 404 for a
 * path it does not route. Run through tests/net/lyrics_status.cmake, which points the client at the fixture. */
#include "devilbox_lyrics.h"

#include <stdio.h>

static int fails;

static void expect(const char* what, int got, int want) {
    if (got != want) {
        printf("FAIL %s: status %d, want %d\n", what, got, want);
        fails++;
    }
}

int main(void) {
    char out[256];
    size_t len = 0;
    int http = 0;

    expect("unknown md5 body", (int)sel_dvb_lyrics_get_ex("ffffffffffffffffffffffffffffffff", out, sizeof out, &len, &http),
           SEL_DVB_LYRICS_UNKNOWN);
    expect("unknown md5 http", http, 404);
    expect("unrouted plain 404", (int)sel_dvb_lyrics_get("00000000000000000000000000000000", out, sizeof out, &len),
           SEL_DVB_LYRICS_UNKNOWN);
    expect("hit", (int)sel_dvb_lyrics_get("d53461365cef8e858993e12b8fcaf1ee", out, sizeof out, &len), SEL_DVB_LYRICS_HIT);
    if (fails != 0) return 1;
    printf("lyrics_status_ok\n");
    return 0;
}
