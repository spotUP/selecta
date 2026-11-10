/* I1300: a raw URL of a known host is the same song as its source key. Unit: sel_song_key_of_url. Import level: the Hippo .prg
 * key reader (sel_hippo_prg_keys, what the HippoPlayer playlist sync stores) gives an ftp.modland.com entry the `modland:` key. */
#include <selecta/host.h>
#include "sources/hippo_prg.h"
#include "song_key.h"
#include <stdio.h>
#include <string.h>

static int fails;

static void expect(const char* in, const char* want) {
    char out[400] = "";

    if (!sel_song_key_of_url(in, out, sizeof out) || strcmp(out, want) != 0) {
        printf("FAIL %s -> %s (want %s)\n", in, out, want);
        fails++;
    }
}

int main(void) {
    static const char prg[] = "HiPPrg\nhttp://ftp.modland.com/pub/modules/AHX/Geir Tjelta/a new beginning.ahx\n"
                              "http://example.org/x/y.mod\nmodland:pub/modules/Protracker/a/b.mod\n";
    char keys[1024];
    const char* want = "modland:pub/modules/AHX/Geir Tjelta/a new beginning.ahx";
    const char* want_keys = "modland:pub/modules/AHX/Geir Tjelta/a new beginning.ahx\n"
                            "http://example.org/x/y.mod\nmodland:pub/modules/Protracker/a/b.mod\n";

    expect("http://ftp.modland.com/pub/modules/AHX/Geir Tjelta/a new beginning.ahx", want);
    expect("https://ftp.modland.com/pub/modules/AHX/Geir Tjelta/a new beginning.ahx", want);
    expect("http://ftp.modland.com/pub/modules/AHX/Geir%20Tjelta/a%20new%20beginning.ahx", want);
    expect("ftp://ftp.modland.com/pub/modules/AHX/Geir%20Tjelta/a%20new%20beginning.ahx", want);
    expect("http://hvsc.c64.org/download/C64Music/MUSICIANS/M/Miami/Miami_Vice.sid", "hvsc:MUSICIANS/M/Miami/Miami_Vice.sid");
    expect("http://hvsc.c64.org/download/C64Music/A/B%20C%281%29.sid", "hvsc:A/B C(1).sid");
    expect("http://example.org/pub/modules/x.mod", "http://example.org/pub/modules/x.mod");
    expect("http://ftp.modland.com/other/x.mod", "http://ftp.modland.com/other/x.mod");
    expect(want, want);
    expect("hvsc:A/B.sid", "hvsc:A/B.sid");
    expect("/Users/x/a%20b.mod", "/Users/x/a%20b.mod");

    if (sel_hippo_prg_keys(sel_host_default(), prg, sizeof prg - 1u, keys, sizeof keys) == 0 || strcmp(keys, want_keys) != 0) {
        printf("FAIL import keys: %s\n", keys);
        fails++;
    }
    if (fails == 0) printf("url_key_ok\n");
    return fails != 0;
}
