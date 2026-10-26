/* I218d: ISO-8859-1 <-> UTF-8 conversion (src/library/latin1.c). */
#include "source/latin1.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(c)                                                \
    do {                                                        \
        if (!(c)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
            failures++;                                         \
        }                                                       \
    } while (0)

int main(void) {
    char u[8], l[8], big[32];
    unsigned c;
    const char* s = "dir/File 09.mod";

    /* Every Latin-1 byte 0xA0..0xFF round-trips through UTF-8 as exactly two bytes. */
    for (c = 0xA0u; c <= 0xFFu; c++) {
        char b = (char)c;

        CHECK(sel_latin1_to_utf8(u, sizeof u, &b, 1) == 2);
        CHECK(sel_utf8_to_latin1(l, sizeof l, u) == 1);
        CHECK((unsigned char)l[0] == c && l[1] == '\0');
    }
    /* ASCII is unchanged both ways. */
    CHECK(sel_latin1_to_utf8(u, sizeof u, "a/b", 3) == 3 && strcmp(u, "a/b") == 0);
    CHECK(sel_utf8_to_latin1(big, sizeof big, s) == 1 && strcmp(big, s) == 0);
    CHECK(sel_latin1_is_ascii(s) == 1);
    CHECK(sel_latin1_is_ascii("Bj\xc3\xb6rk") == 0);
    /* Above U+00FF there is no Latin-1 spelling: the call fails and leaves an empty string, never '?'. */
    CHECK(sel_utf8_to_latin1(l, sizeof l, "\xc4\x80") == 0 && l[0] == '\0');      /* U+0100 */
    CHECK(sel_utf8_to_latin1(l, sizeof l, "a\xe2\x82\xac") == 0 && l[0] == '\0'); /* euro sign */
    CHECK(sel_utf8_to_latin1(l, sizeof l, "\xf0\x9f\x8e\xb5") == 0);              /* U+1F3B5 */
    /* Malformed input: lone continuation, truncated sequence, overlong C0 80. */
    CHECK(sel_utf8_to_latin1(l, sizeof l, "\x80") == 0);
    CHECK(sel_utf8_to_latin1(l, sizeof l, "\xc3") == 0);
    CHECK(sel_utf8_to_latin1(l, sizeof l, "\xc0\x80") == 0);
    /* Capacity: Latin-1 output that does not fit fails; the UTF-8 side truncates at a whole character. */
    CHECK(sel_utf8_to_latin1(l, 3, "abc") == 0);
    CHECK(sel_utf8_to_latin1(l, 4, "abc") == 1);
    CHECK(sel_latin1_to_utf8(u, 4, "a\xf6\xf6", 3) == 3 && strcmp(u, "a\xc3\xb6") == 0);
    CHECK(sel_latin1_to_utf8(u, 0, "a", 1) == 0);

    if (failures == 0) {
        printf("latin1: ok\n");
    }
    return failures != 0;
}
