/* T29 playlist-codecs: Hippo .prg, M3U, PLS and XSPF read and write
 * (plan M1.2 row I72). Behaviour lives in src/library/playlist; the verbs
 * PLAYLIST_LOAD and PLAYLIST_SAVE (row I73) are tested by their own script. */
#include "playlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

typedef struct {
    char buf[8192];
    size_t len;
} capture;

static int cap_sink(void* ctx, const char* bytes, uint32_t len) {
    capture* c = ctx;
    if (c->len + len >= sizeof c->buf) {
        return 0;
    }
    memcpy(c->buf + c->len, bytes, len);
    c->len += len;
    c->buf[c->len] = '\0';
    return 1;
}

static char warnings[256];

static void warn_cb(void* ctx, const char* msg) {
    (void)ctx;
    strcat(warnings, msg);
    strcat(warnings, ";");
}

static sel_pl_entry entries[8];
static char text[2048];

static void fresh(sel_playlist* pl) {
    sel_playlist_init(pl, entries, 8, text, sizeof text);
    warnings[0] = '\0';
}

static const char* key_of(const sel_playlist* pl, uint32_t i) {
    return sel_playlist_str(pl, pl->entries[i].key);
}

static const char* name_of(const sel_playlist* pl, uint32_t i) {
    return sel_playlist_str(pl, pl->entries[i].name);
}

/* Header, a named URL, a divider (0xF7), a stealth entry (0xA2), a subsong. */
static const char fixture[] = "HiPPrg\n\n"
                              "http://x.org/radio.mp3#name=Radio One\n"
                              "\xF7"
                              "Chiptunes\n"
                              "\xA2"
                              "/mods/hidden.mod\n"
                              "/mods/b.mod#song=2\n";

static void test_hippo(void) {
    sel_playlist pl;
    capture out;
    int rc;
    memset(&out, 0, sizeof out);
    fresh(&pl);
    rc = sel_playlist_read(SEL_PL_AUTO, fixture, sizeof fixture - 1, &pl, warn_cb, NULL);
    CHECK(rc == SEL_PL_OK, "hippo read rc=%d", rc);
    CHECK(pl.count == 4, "hippo count=%u", (unsigned)pl.count);
    if (pl.count != 4) {
        return;
    }
    CHECK(pl.entries[0].kind == SEL_PL_SONG && pl.entries[1].kind == SEL_PL_DIVIDER && pl.entries[2].kind == SEL_PL_STEALTH &&
              pl.entries[3].kind == SEL_PL_SONG,
          "hippo kinds");
    CHECK(strcmp(key_of(&pl, 0), "http://x.org/radio.mp3") == 0 && strcmp(name_of(&pl, 0), "Radio One") == 0, "entry 0 key/name");
    CHECK(strcmp(name_of(&pl, 1), "Chiptunes") == 0, "divider name");
    CHECK(strcmp(key_of(&pl, 2), "/mods/hidden.mod") == 0, "stealth key");
    CHECK(strcmp(key_of(&pl, 3), "/mods/b.mod") == 0 && pl.entries[3].subsong == 2, "subsong entry");
    rc = sel_playlist_write(SEL_PL_HIPPO, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_OK, "hippo write rc=%d", rc);
    CHECK(out.len == sizeof fixture - 1 && memcmp(out.buf, fixture, out.len) == 0, "hippo write not byte-identical (len %u)",
          (unsigned)out.len);
    CHECK(warnings[0] == '\0', "hippo write warned: %s", warnings);
}

static void test_hippo_range(void) {
    static sel_pl_entry big[SEL_PL_HIPPO_MAX + 1u];
    sel_playlist pl;
    capture out;
    uint32_t i;
    int rc;
    memset(&out, 0, sizeof out);
    sel_playlist_init(&pl, big, SEL_PL_HIPPO_MAX + 1u, text, sizeof text);
    for (i = 0; i < SEL_PL_HIPPO_MAX + 1u; i++) {
        CHECK(sel_playlist_add(&pl, SEL_PL_SONG, "", 0, "", 0, 0, 0) == SEL_PL_OK, "add %u", (unsigned)i);
    }
    CHECK(pl.count == 131072u, "count %u", (unsigned)pl.count);
    rc = sel_playlist_write(SEL_PL_HIPPO, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_RANGE, "131072 entries: rc=%d", rc);
    pl.count = SEL_PL_HIPPO_MAX;
    out.len = 0;
    rc = sel_playlist_write(SEL_PL_HIPPO, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_SINK || rc == SEL_PL_OK, "131071 entries are in range: rc=%d", rc);
}

static void read_fixture(sel_playlist* pl) {
    int rc;
    fresh(pl);
    rc = sel_playlist_read(SEL_PL_HIPPO, fixture, sizeof fixture - 1, pl, NULL, NULL);
    CHECK(rc == SEL_PL_OK && pl->count == 4, "fixture read");
    pl->entries[0].duration_ms = 185000;
}

static void test_m3u(void) {
    sel_playlist pl, back;
    capture out;
    int rc;
    memset(&out, 0, sizeof out);
    read_fixture(&pl);
    warnings[0] = '\0';
    rc = sel_playlist_write(SEL_PL_M3U, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_OK, "m3u write rc=%d", rc);
    CHECK(strncmp(out.buf, "#EXTM3U\n", 8) == 0, "m3u header");
    CHECK(strstr(out.buf, "#EXTINF:185,Radio One\nhttp://x.org/radio.mp3\n") != NULL, "m3u extinf: %s", out.buf);
    CHECK(strstr(warnings, "lossy divider") != NULL, "m3u divider warning: '%s'", warnings);
    CHECK(strstr(out.buf, "Chiptunes") == NULL, "divider leaked into m3u");
    fresh(&back);
    rc = sel_playlist_read(SEL_PL_AUTO, out.buf, out.len, &back, NULL, NULL);
    CHECK(rc == SEL_PL_OK && back.count == 3, "m3u read count=%u", (unsigned)back.count);
    if (back.count == 3) {
        CHECK(strcmp(key_of(&back, 0), "http://x.org/radio.mp3") == 0 && back.entries[0].duration_ms == 185000 &&
                  strcmp(name_of(&back, 0), "Radio One") == 0,
              "m3u entry 0");
        CHECK(strcmp(key_of(&back, 1), "/mods/hidden.mod") == 0 && strcmp(key_of(&back, 2), "/mods/b.mod") == 0, "m3u keys");
    }
    /* CRLF and a plain list without #EXTM3U */
    fresh(&back);
    rc = sel_playlist_read(SEL_PL_M3U, "a.mod\r\n\r\nb.mod\r\n", 15, &back, NULL, NULL);
    CHECK(rc == SEL_PL_OK && back.count == 2 && strcmp(key_of(&back, 1), "b.mod") == 0, "plain m3u");
}

static void test_pls(void) {
    sel_playlist pl, back;
    capture out;
    int rc;
    memset(&out, 0, sizeof out);
    fresh(&pl);
    sel_playlist_add(&pl, SEL_PL_SONG, "/a.mod", 6, "Alpha", 5, 0, 61000);
    sel_playlist_add(&pl, SEL_PL_SONG, "/b.mod", 6, "", 0, 0, 0);
    sel_playlist_add(&pl, SEL_PL_STEALTH, "/c.mod", 6, "", 0, 0, 0);
    sel_playlist_add(&pl, SEL_PL_SONG, "/d.mod", 6, "", 0, 0, 0);
    rc = sel_playlist_write(SEL_PL_PLS, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_OK, "pls write rc=%d", rc);
    CHECK(strncmp(out.buf, "[playlist]\n", 11) == 0, "pls header");
    CHECK(strstr(out.buf, "NumberOfEntries=4\n") != NULL && strstr(out.buf, "Version=2\n") != NULL, "pls trailer: %s", out.buf);
    CHECK(strstr(out.buf, "File1=/a.mod\nTitle1=Alpha\nLength1=61\n") != NULL, "pls entry 1: %s", out.buf);
    fresh(&back);
    rc = sel_playlist_read(SEL_PL_AUTO, out.buf, out.len, &back, NULL, NULL);
    CHECK(rc == SEL_PL_OK && back.count == 4, "pls read count=%u", (unsigned)back.count);
    if (back.count == 4) {
        CHECK(strcmp(key_of(&back, 3), "/d.mod") == 0 && strcmp(name_of(&back, 0), "Alpha") == 0 && back.entries[0].duration_ms == 61000,
              "pls fields");
    }
    read_fixture(&pl);
    out.len = 0;
    rc = sel_playlist_write(SEL_PL_PLS, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_OK && strstr(warnings, "lossy divider") != NULL, "pls divider warning");
}

static void test_xspf(void) {
    sel_playlist pl, back;
    capture out;
    int rc;
    memset(&out, 0, sizeof out);
    read_fixture(&pl);
    sel_playlist_add(&pl, SEL_PL_SONG, "/m&m/<x>.mod", 12, "A & B", 5, 0, 0);
    warnings[0] = '\0';
    rc = sel_playlist_write(SEL_PL_XSPF, &pl, cap_sink, &out, warn_cb, NULL);
    CHECK(rc == SEL_PL_OK, "xspf write rc=%d", rc);
    CHECK(strstr(out.buf, "<location>/mods/b.mod</location>") != NULL, "xspf location: %s", out.buf);
    CHECK(strstr(out.buf, "<extension application=\"selecta\"><subsong>2</subsong></extension>") != NULL, "xspf subsong");
    CHECK(strstr(out.buf, "<location>/m&amp;m/&lt;x&gt;.mod</location>") != NULL, "xspf escaping");
    CHECK(strstr(warnings, "lossy divider") != NULL, "xspf divider warning");
    fresh(&back);
    rc = sel_playlist_read(SEL_PL_AUTO, out.buf, out.len, &back, NULL, NULL);
    CHECK(rc == SEL_PL_OK && back.count == 4, "xspf read count=%u", (unsigned)back.count);
    if (back.count == 4) {
        CHECK(strcmp(key_of(&back, 0), "http://x.org/radio.mp3") == 0 && back.entries[0].duration_ms == 185000, "xspf entry 0");
        CHECK(strcmp(key_of(&back, 2), "/mods/b.mod") == 0 && back.entries[2].subsong == 2, "xspf subsong back");
        CHECK(strcmp(key_of(&back, 3), "/m&m/<x>.mod") == 0 && strcmp(name_of(&back, 3), "A & B") == 0, "xspf unescape");
    }
}

static void test_detect(void) {
    CHECK(sel_playlist_format_from_name("a/List.PRG") == SEL_PL_HIPPO, "ext prg");
    CHECK(sel_playlist_format_from_name("x.m3u8") == SEL_PL_M3U, "ext m3u8");
    CHECK(sel_playlist_format_from_name("x.pls") == SEL_PL_PLS, "ext pls");
    CHECK(sel_playlist_format_from_name("x.xspf") == SEL_PL_XSPF, "ext xspf");
    CHECK(sel_playlist_format_from_name("x.txt") == SEL_PL_AUTO, "ext unknown");
    CHECK(sel_playlist_sniff("HiPPrg\n", 7) == SEL_PL_HIPPO, "sniff hippo");
    CHECK(sel_playlist_sniff("#EXTM3U\n", 8) == SEL_PL_M3U, "sniff m3u");
    CHECK(sel_playlist_sniff("[playlist]\n", 11) == SEL_PL_PLS, "sniff pls");
    CHECK(sel_playlist_sniff("<?xml ", 6) == SEL_PL_XSPF, "sniff xspf");
    CHECK(sel_playlist_sniff("zzz", 3) == SEL_PL_AUTO, "sniff none");
}

int main(void) {
    test_hippo();
    test_hippo_range();
    test_m3u();
    test_pls();
    test_xspf();
    test_detect();
    printf("playlist_test failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
