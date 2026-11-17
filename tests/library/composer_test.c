/* I182a: golden cases for the composer canonical key and display name (src/library/composer.c). The composer query,
 * the stored song_composers rows and the composer page header all go through these two functions. */
#include "composer.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void canon_is(int line, const char* name, const char* want) {
    char out[SEL_COMPOSER_MAX];
    int r = sel_composer_canon(name, out, sizeof out);

    if (strcmp(out, want) != 0 || r != (want[0] != '\0')) {
        printf("FAIL line %d: canon('%s') = '%s' (r=%d), want '%s'\n", line, name ? name : "(null)", out, r, want);
        g_fail++;
    }
}

static void display_is(int line, const char* name, const char* want) {
    char out[SEL_COMPOSER_MAX];
    int r = sel_composer_display(name, out, sizeof out);

    if (strcmp(out, want) != 0 || r != (want[0] != '\0')) {
        printf("FAIL line %d: display('%s') = '%s' (r=%d), want '%s'\n", line, name ? name : "(null)", out, r, want);
        g_fail++;
    }
}

#define CANON(n, w) canon_is(__LINE__, n, w)
#define DISPLAY(n, w) display_is(__LINE__, n, w)

int main(void) {
    static const char* const same[] = {"Rob Hubbard", "Hubbard, Rob", "  ROB   HUBBARD ", "rob.hubbard",
                                       "Rob Hubbard (RH)", "Hubbard,Rob", "rob\thubbard"};
    size_t i;

    for (i = 0; i < sizeof same / sizeof same[0]; i++) CANON(same[i], "rob hubbard");
    CANON("Jeroen Tel (JT)", "jeroen tel");
    CANON("(Luca Pirovano) Lizardking", "lizardking");
    CANON("Laxity/Vibrants", "laxity vibrants");
    CANON("4-Mat", "4 mat");
    CANON("a, b, c", "a b c");                 /* two commas: no swap */
    CANON("J\xc3\xb6rg Stra\xc3\x9f", "j\xc3\xb6rg stra\xc3\x9f"); /* UTF-8 bytes pass through, ASCII folds */
    CANON("\xc3\x96RJAN", "\xc3\xb6rjan");     /* I182b: Latin-1 capitals fold in UTF-8 */
    CANON("\xc3\x85SA \xc3\x84NG", "\xc3\xa5sa \xc3\xa4ng");
    CANON("\xc3\xb6rjan", "\xc3\xb6rjan");     /* lowercase stays */
    CANON("A\xc3\x97" "B", "a\xc3\x97" "b");    /* U+00D7 multiplication sign is not a letter pair */
    CANON("\xc3\x96 \xc3\xb6", "\xc3\xb6 \xc3\xb6");
    CANON("", "");
    CANON("(handle)", "");
    CANON(" -- ", "");
    CANON(NULL, "");

    /* A short output buffer truncates on a word boundary without a trailing space. */
    {
        char out[5];

        sel_composer_canon("Rob Hubbard", out, sizeof out);
        if (strcmp(out, "rob") != 0) {
            printf("FAIL line %d: canon cap 5 = '%s', want 'rob'\n", __LINE__, out);
            g_fail++;
        }
    }

    DISPLAY("LILJEDAHL, ANDERS", "Anders Liljedahl");
    DISPLAY("Rob HUBBARD (RH)", "Rob Hubbard");
    DISPLAY("JT", "JT");                       /* fewer than four letters stay as written */
    DISPLAY("  Jeroen   Tel ", "Jeroen Tel");
    DISPLAY("", "");
    DISPLAY(NULL, "");

    /* The display of a name has the same canonical key as the name. */
    {
        static const char* const names[] = {"LILJEDAHL, ANDERS", "Rob HUBBARD (RH)", "4-Mat", "Hubbard,Rob"};

        for (i = 0; i < sizeof names / sizeof names[0]; i++) {
            char d[SEL_COMPOSER_MAX], a[SEL_COMPOSER_MAX], b[SEL_COMPOSER_MAX];

            sel_composer_display(names[i], d, sizeof d);
            sel_composer_canon(names[i], a, sizeof a);
            sel_composer_canon(d, b, sizeof b);
            if (strcmp(a, b) != 0) {
                printf("FAIL line %d: canon('%s') = '%s' but canon(display) = '%s'\n", __LINE__, names[i], a, b);
                g_fail++;
            }
        }
    }
    /* I1443: a SID header's author/released fields -> artist, year, group (the playsid player reports them as AUTHOR and RELEASE) */
    {
        static const struct {
            const char *format, *author, *released, *artist, *group;
            int32_t year;
        } c[] = {
            {"SID", "<?>", "1988 Fairlight", "Fairlight", "Fairlight", 1988},
            {"SID 2SID", "Rob Hubbard", "1985 Gremlin Graphics", "Rob Hubbard", "Gremlin Graphics", 1985},
            {"SID", "", "198? Censor Design ", "Censor Design", "Censor Design", 0},
            {"SID", "<?>", "1990", "", "", 1990},
        };
        size_t i;
        char artist[64], group[64];
        int32_t year;

        for (i = 0; i < sizeof c / sizeof c[0]; i++) {
            int r = sel_sid_credit(c[i].format, c[i].author, c[i].released, artist, sizeof artist, group, sizeof group, &year);
            if (!r || strcmp(artist, c[i].artist) != 0 || strcmp(group, c[i].group) != 0 || year != c[i].year) {
                printf("FAIL line %d: sid credit %zu = '%s' '%s' %d\n", __LINE__, i, artist, group, (int)year);
                g_fail++;
            }
        }
        if (sel_sid_credit("MOD", "x", "1988 y", artist, sizeof artist, group, sizeof group, &year)) {
            printf("FAIL line %d: sid credit accepted a non-SID format\n", __LINE__);
            g_fail++;
        }
    }
    if (g_fail == 0) printf("composer_ok\n");
    return g_fail != 0;
}
