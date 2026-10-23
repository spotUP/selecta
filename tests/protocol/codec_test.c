/* codec-optional-arg: a trailing `?` in a commands.def signature makes only
 * the last argument optional (commands.def header). Regression for the
 * decoder that counted `?` so the argument before it stayed required. */
#include <selecta/protocol.h>

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char* line, int want_rc, unsigned want_argc) {
    sel_message m;
    char pool[256];
    int rc = sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool);
    if (rc != want_rc) {
        failures++;
        printf("FAIL '%s': rc %s, want %s\n", line, sel_codec_strerror(rc), sel_codec_strerror(want_rc));
        return;
    }
    if (rc == SEL_CODEC_OK && m.argc != want_argc) {
        failures++;
        printf("FAIL '%s': argc %u, want %u\n", line, (unsigned)m.argc, want_argc);
    }
}

int main(void) {
    /* "s?": zero or one argument. */
    expect("QUEUE_SHUFFLE", SEL_CODEC_OK, 0);
    expect("QUEUE_SHUFFLE 7", SEL_CODEC_OK, 1);
    expect("QUEUE_SHUFFLE 7 8", SEL_CODEC_E_ARG_COUNT, 0);
    /* "ss?": first required, second optional. */
    expect("SHUFFLE", SEL_CODEC_E_ARG_COUNT, 0);
    expect("SHUFFLE on", SEL_CODEC_OK, 1);
    expect("SHUFFLE on 42", SEL_CODEC_OK, 2);
    /* "uu?": same rule on unsigned arguments. */
    expect("SNAPSHOT_QUEUE 0", SEL_CODEC_OK, 1);
    expect("SNAPSHOT_QUEUE 0 10", SEL_CODEC_OK, 2);
    /* "s": a plain signature stays exact. */
    expect("OPEN", SEL_CODEC_E_ARG_COUNT, 0);
    expect("OPEN a.mod", SEL_CODEC_OK, 1);
    printf("%s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
