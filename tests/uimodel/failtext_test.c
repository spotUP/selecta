/* I1383b: sel_uim_open_fail_text, the shared failed-open words. */
#include <selecta/uimodel/failtext.h>

#include <stdio.h>
#include <string.h>

static int g_fail;
static void expect(const char* code, const char* detail, int want_ret, const char* want) {
    char out[96] = "untouched";
    int r = sel_uim_open_fail_text(code, detail, out, sizeof out);
    if (r != want_ret || strcmp(out, want) != 0) {
        g_fail++;
        printf("FAIL %s | %s -> %d \"%s\"\n", code, detail != NULL ? detail : "(null)", r, out);
    }
}

int main(void) {
    expect("no_replayer", "format=pt3 modland:Foo/bar.pt3", 1, "No player for PT3 yet");
    expect("no_replayer", "format=unknown /dev/null", 1, "No player for that file type yet");
    expect("no_replayer", NULL, 1, "No player for that file type yet");
    expect("open_failed", "http_status=404 http://x/y", 1, "That song could not be downloaded (HTTP 404)");
    expect("open_failed", "http_status=403 Zxart blocks downloads of this song", 1,
           "That song could not be downloaded: Zxart blocks downloads of this song (HTTP 403)");
    expect("open_failed", "x", 1, "That song cannot be played (x)");
    expect("replayer_load_failed", "player.lib: bad hunk", 1, "That song cannot be played (player.lib: bad hunk)");
    expect("open_failed", "", 1, "That song cannot be played");
    expect("open_failed", NULL, 1, "That song cannot be played");
    expect("something_else", "x", 0, "untouched");
    if (g_fail == 0) {
        printf("uim_failtext_ok\n");
    }
    return g_fail != 0;
}
