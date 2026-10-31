/* format-label-unit (I819): sel_format_label, the one display label of a song format. */
#include <selecta/format_label.h>

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char* raw, const char* want) {
    char out[SEL_FORMAT_LABEL_MAX];
    sel_format_label(raw, out, sizeof out);
    if (strcmp(out, want) != 0) {
        printf("FAIL '%s' -> '%s', want '%s'\n", raw != NULL ? raw : "(null)", out, want);
        failures++;
    }
}

int main(void) {
    char small[8];
    expect("Protracker", "Protracker (mod)");
    expect("fasttracker 2", "Fasttracker 2 (xm)"); /* a name matches without case and takes the table's casing */
    expect("xm", "Fasttracker 2 (xm)");
    expect(".XM", "Fasttracker 2 (xm)");
    expect("mod", "Protracker (mod)"); /* Startrekker shares the extension: the first table entry wins */
    expect("Startrekker", "Startrekker (mod)");
    expect("bp3", "SoundMon 3 (bp3)");    /* a UADE prefix is the entry's extension */
    expect("Radio - MP3", "Radio - MP3"); /* unknown formats keep their plain name */
    expect("flac", "flac");
    expect("Protracker (mod)", "Protracker (mod)"); /* idempotent */
    expect("", "");
    expect(NULL, "");
    if (sel_format_label("Game Boy Sound Format", small, sizeof small) != sizeof small - 1u || small[sizeof small - 1u] != '\0') {
        printf("FAIL truncation\n");
        failures++;
    }
    if (strlen("Game Boy Sound Format (gbs)") >= SEL_FORMAT_LABEL_MAX) {
        printf("FAIL SEL_FORMAT_LABEL_MAX too small\n");
        failures++;
    }
    if (failures == 0) {
        printf("format_label_ok\n");
    }
    return failures != 0;
}
