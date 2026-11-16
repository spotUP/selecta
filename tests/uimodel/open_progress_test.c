/* I1415: sel_open_progress_text / sel_open_progress_fraction for every phase and the unknown-total case. */
#include <selecta/uimodel/open_progress.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(const sel_open_progress* p, const char* want, int32_t want_frac) {
    char buf[64];

    sel_open_progress_text(p, buf, sizeof buf);
    if (strcmp(buf, want) != 0 || sel_open_progress_fraction(p) != want_frac) {
        printf("FAIL: phase %u got \"%s\" frac %d, want \"%s\" frac %d\n", (unsigned)p->phase, buf, (int)sel_open_progress_fraction(p), want,
               (int)want_frac);
        failures++;
    }
}

int main(void) {
    sel_open_progress p;
    char tiny[4];

    memset(&p, 0, sizeof p);
    check(&p, "", -1);
    p.active = 1;
    p.phase = SEL_OPEN_CONNECTING;
    check(&p, "Connecting", -1);
    p.phase = SEL_OPEN_DOWNLOADING;
    p.total = 1000u;
    p.done = 420u;
    check(&p, "Downloading 42%", 27525); /* 420 * 65536 / 1000 */
    p.done = 5000u;
    check(&p, "Downloading 100%", 65536); /* done past total clamps */
    p.files = 3u;
    p.file = 2u;
    p.done = 420u;
    check(&p, "Downloading file 2 of 3 - 42%", 27525);
    p.files = 0u;
    p.total = 0u;
    p.done = 1572864u;
    check(&p, "Downloading 1.5 MB", -1);
    p.done = 2048u;
    check(&p, "Downloading 2 KB", -1);
    p.done = 12u;
    check(&p, "Downloading 12 B", -1);
    p.phase = SEL_OPEN_UNPACKING;
    check(&p, "Unpacking", -1);
    p.phase = SEL_OPEN_STARTING;
    check(&p, "Starting", -1);
    sel_open_progress_text(&p, tiny, sizeof tiny);
    if (strcmp(tiny, "Sta") != 0) {
        printf("FAIL: cut text \"%s\"\n", tiny);
        failures++;
    }
    if (failures == 0) {
        printf("uim_open_progress_ok\n");
    }
    return failures != 0;
}
