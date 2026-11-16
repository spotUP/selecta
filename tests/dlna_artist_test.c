/* I1410: the DLNA artist fallback (plugins/sources/dlna/dlna_artist.h). */
#include "dlna_artist.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(const char* tag, const char* title, const char* want_artist, const char* want_title) {
    char a[256], t[256];

    sel_dlna_artist(tag, title, a, t, sizeof a);
    if (strcmp(a, want_artist) != 0 || strcmp(t, want_title) != 0) {
        printf("FAIL '%s' '%s' -> '%s' / '%s'\n", tag, title, a, t);
        failures++;
    }
}

int main(void) {
    check("", "Mortimer_Twang_-_Amanda", "Mortimer Twang", "Amanda"); /* underscore split */
    check("", "Mortimer Twang - Amanda", "Mortimer Twang", "Amanda"); /* spaced split */
    check("", "Some Tune", SEL_DLNA_UNKNOWN_ARTIST, "Some Tune");     /* no split */
    check("", "Tune - ", SEL_DLNA_UNKNOWN_ARTIST, "Tune - ");         /* empty title part */
    check("Real Artist", "A_-_B", "Real Artist", "A_-_B");            /* real artist wins */
    if (failures == 0) {
        printf("dlna_artist_ok\n");
    }
    return failures != 0;
}
