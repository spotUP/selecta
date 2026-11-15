/* I1387: sel_uim_tidy_title, table driven. */
#include <selecta/uimodel/title.h>
#include <stdio.h>
#include <string.h>

static const struct { const char* in; const char* want; } T[] = {
    {"Page_123_male_vocals", "Page 123 male vocals"},
    {"MusicLikeThis", "Music Like This"},
    {"Music_LikeThis", "Music LikeThis"},
    {"ProTracker", "ProTracker"},
    {"McDonald", "McDonald"},
    {"McDonaldBurgerKing", "McDonald Burger King"},
    {"ThatsALotOfStuff", "Thats A Lot Of Stuff"}, /* capital run before a word */
    {"DJsMix", "DJsMix"},
    {"MacDonaldBurgerKing", "MacDonald Burger King"},
    {"DJ", "DJ"},
    {"DJShadowMusicBox", "DJ Shadow Music Box"},
    {"Music Like This", "Music Like This"},
    {"Music LikeThisToo", "Music LikeThisToo"},
    {"__a__b__", "A B"},
    {"  spaced   out  ", "Spaced Out"},
    {"", ""},
    {"_", ""},
    {"iPhoneCaseRed", "iPhoneCaseRed"},
    {"Caf\xC3\xA9_Del_Mar", "Caf\xC3\xA9 Del Mar"},
    {"song2NextPart", "song2NextPart"},
    {"AbCdEf", "Ab Cd Ef"},
    {"page_123_male_vocals", "Page 123 Male Vocals"},
    {"2nd song of 1999", "2nd Song Of 1999"},
    {"caf\xC3\xA9 del mar", "Caf\xC3\xA9 Del Mar"},
    {"already Mixed case", "already Mixed case"},
    {"123", "123"},
};

int main(void) {
    int fail = 0;
    size_t i;
    char buf[64], small[6];
    for (i = 0; i < sizeof T / sizeof T[0]; i++) {
        sel_uim_tidy_title(T[i].in, buf, sizeof buf);
        if (strcmp(buf, T[i].want) != 0) {
            printf("FAIL \"%s\" -> \"%s\" want \"%s\"\n", T[i].in, buf, T[i].want);
            fail++;
        }
    }
    sel_uim_tidy_title("Music_Like_This", small, sizeof small); /* cut to cap-1 */
    if (strcmp(small, "Music") != 0) {
        printf("FAIL small \"%s\"\n", small);
        fail++;
    }
    sel_uim_tidy_title("abc\xC3\xA9", small, 5u); /* never half a UTF-8 sequence */
    if (strcmp(small, "Abc") != 0) {
        printf("FAIL utf8 cut \"%s\"\n", small);
        fail++;
    }
    sel_uim_tidy_title(NULL, buf, sizeof buf);
    fail += buf[0] != '\0';
    snprintf(buf, sizeof buf, "%s", "page_1_x"); /* I1387b: only name rows are tidied */
    sel_uim_tidy_info("Title", buf, sizeof buf);
    fail += strcmp(buf, "Page 1 X") != 0;
    snprintf(buf, sizeof buf, "%s", "page_1_x");
    sel_uim_tidy_info("Format", buf, sizeof buf);
    fail += strcmp(buf, "page_1_x") != 0;
    if (fail == 0) {
        printf("uim_title_ok\n");
    }
    return fail != 0;
}
