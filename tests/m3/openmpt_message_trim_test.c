/* I774t: the libopenmpt "message" of a MOD is 31 sample names joined by newlines. trim_blank_lines drops the empty
 * lines at both ends and folds a run of empty lines in the middle into one. The adapter is included for its static helper. */
#include "../../plugins/replayers/openmpt/adapter.c"

#include <stdio.h>

static int check(const char* in, const char* want) {
    char buf[256];
    snprintf(buf, sizeof buf, "%s", in);
    trim_blank_lines(buf);
    if (strcmp(buf, want) != 0) {
        fprintf(stderr, "FAIL: [%s] -> [%s], want [%s]\n", in, buf, want);
        return 1;
    }
    return 0;
}

int main(void) {
    int bad = 0;
    bad |= check("\n\n\nsample one\n\n\n\n\nsample two\n\n\n\n", "sample one\n\nsample two");
    bad |= check("a\nb\nc\n", "a\nb\nc");
    bad |= check("a\r\n\r\n\r\nb", "a\n\nb");
    bad |= check("\n\n", "");
    bad |= check("", "");
    if (!bad) puts("openmpt_message_trim_ok");
    return bad;
}
