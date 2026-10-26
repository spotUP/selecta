/* I444: a Latin-1 composer name from a module ("Andr\xE9 Eickler", Face The Music rock.ftm) reached the library
 * and the screen as raw bytes and drew as "Andr" plus a box. Plugin metadata text is now converted to UTF-8
 * where it enters the host; valid UTF-8 passes through untouched. */
#include "source/latin1.h"
#include "meta_utf8.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); g_fail++; } } while (0)

static void test_validator(void) {
    CHECK(sel_utf8_valid(""));
    CHECK(sel_utf8_valid("plain"));
    CHECK(sel_utf8_valid("Andr\xC3\xA9"));
    CHECK(sel_utf8_valid("\xE2\x82\xAC \xF0\x9F\x8E\xB5"));
    CHECK(!sel_utf8_valid("Andr\xE9 Eickler"));    /* Latin-1 e-acute */
    CHECK(!sel_utf8_valid("\xC3"));                /* truncated */
    CHECK(!sel_utf8_valid("\xC0\xAF"));            /* overlong */
    CHECK(!sel_utf8_valid("\xE0\x80\xAF"));        /* overlong, three bytes */
    CHECK(!sel_utf8_valid("\xED\xA0\x80"));        /* surrogate */
    CHECK(!sel_utf8_valid("\xF4\x90\x80\x80"));    /* above U+10FFFF */
    CHECK(!sel_utf8_valid("\x80"));                /* stray continuation */
}

static void test_text(void) {
    char d[16];

    CHECK(sel_text_to_utf8(d, sizeof d, "Andr\xE9 Eickler") == 14 && strcmp(d, "Andr\xC3\xA9 Eickler") == 0);
    CHECK(sel_text_to_utf8(d, sizeof d, "Andr\xC3\xA9") == 6 && strcmp(d, "Andr\xC3\xA9") == 0);
    /* valid UTF-8 cut at a character boundary, never inside one */
    CHECK(sel_text_to_utf8(d, 6, "Andr\xC3\xA9") == 4 && strcmp(d, "Andr") == 0);
}

static void test_meta_list(void) {
    sel_tag out[SEL_META_UTF8_TAGS];
    char text[64];
    const sel_tag tail[] = {{SEL_META_FORMAT, (uintptr_t)"Face The Music"}, {SEL_TAG_END, 0}};
    const sel_tag bad[] = {{SEL_META_TITLE, (uintptr_t)"rock"},
                           {SEL_META_AUTHOR, (uintptr_t)"Andr\xE9 Eickler"},
                           {SEL_META_DURATION_MS, 1234},
                           {SEL_TAG_MORE, (uintptr_t)tail}};
    const sel_tag good[] = {{SEL_META_AUTHOR, (uintptr_t)"Andr\xC3\xA9 Eickler"}, {SEL_TAG_END, 0}};
    const sel_tag* r;

    /* the composer name the Composers list shows */
    r = sel_meta_utf8(bad, out, SEL_META_UTF8_TAGS, text, sizeof text);
    CHECK(r == out);
    CHECK(strcmp((const char*)sel_tag_find(r, SEL_META_AUTHOR, 0), "Andr\xC3\xA9 Eickler") == 0);
    CHECK(strcmp((const char*)sel_tag_find(r, SEL_META_TITLE, 0), "rock") == 0);
    CHECK(strcmp((const char*)sel_tag_find(r, SEL_META_FORMAT, 0), "Face The Music") == 0); /* MORE followed */
    CHECK(sel_tag_find(r, SEL_META_DURATION_MS, 0) == 1234u);                               /* INT untouched */
    CHECK(sel_tag_count(r) == 4u);
    /* valid UTF-8: the replayer's own list comes back, nothing copied */
    CHECK(sel_meta_utf8(good, out, SEL_META_UTF8_TAGS, text, sizeof text) == good);
    /* no room for the list: unchanged rather than cut */
    CHECK(sel_meta_utf8(bad, out, 3, text, sizeof text) == bad);
    /* text buffer too small: converted truncated, still valid UTF-8 */
    r = sel_meta_utf8(bad, out, SEL_META_UTF8_TAGS, text, 6);
    CHECK(sel_utf8_valid((const char*)sel_tag_find(r, SEL_META_AUTHOR, 0)));
}

int main(void) {
    test_validator();
    test_text();
    test_meta_list();
    if (g_fail == 0) puts("meta_utf8_ok");
    return g_fail ? 1 : 0;
}
