/* I1314c: the Amiga nuade manifest fills a scan row's title and author from the song header (scan_text.c). One tiny
 * synthetic header per parser, each with its own title text: the signature table must name the player and the parser
 * must read the field where that player's loader reads it. */
#include "signatures.h"
#include "scan_text.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void put(unsigned char* b, size_t at, const char* s) {
    memcpy(b + at, s, strlen(s));
}

static void check(const char* label, const unsigned char* b, size_t size, const char* want_id, const char* want_title,
                  const char* want_author) {
    char title[64], author[64];
    const char* id = sel_nuade_sig_match(b, size);
    if (id == NULL || strcmp(id, want_id) != 0) {
        printf("FAIL %s: sig id %s, want %s\n", label, id != NULL ? id : "(none)", want_id);
        g_fail++;
        return;
    }
    if (!sel_nuade_scan_text(id, b, size, title, sizeof title, author, sizeof author) || strcmp(title, want_title) != 0 ||
        strcmp(author, want_author) != 0) {
        printf("FAIL %s: title '%s' author '%s', want '%s' '%s'\n", label, title, author, want_title, want_author);
        g_fail++;
    }
}

int main(void) {
    static unsigned char b[0x1F0E + 512];
    char t[8], a[8];

    memset(b, 0, sizeof b);
    put(b, 0, "Soundmon Tune   ");
    put(b, 26, "V.2");
    check("soundmon", b, 512, "soundmon", "Soundmon Tune", "");

    memset(b, 0, sizeof b);
    put(b, 0, "FTMN");
    b[4] = 3;
    b[14] = 6;  /* speed */
    b[15] = 16; /* rows per measure = 96 / speed */
    put(b, 16, "Face Title");
    put(b, 48, "Face Artist");
    b[48 + 11] = 0xE9; /* Latin-1 e acute -> UTF-8 C3 A9 */
    check("facethemusic", b, 512, "facethemusic", "Face Title", "Face Artist\xC3\xA9");

    memset(b, 0, sizeof b);
    put(b, 0, " MUGICIAN/SOFTEYES 1990 ");
    put(b, 80, "Mugi Sub One");
    check("digmug", b, 512, "digmug", "Mugi Sub One", "");

    memset(b, 0, sizeof b);
    put(b, 0, "ISM!V1.2");
    put(b, 36, "InStereo Name");
    check("instereo1", b, 512, "instereo1", "InStereo Name", "");

    memset(b, 0, sizeof b);
    put(b, 0, "Synth4.0");
    put(b, 36, "Synth Forty");
    check("synthesis 4.0", b, 512, "synthesis", "Synth Forty", "");

    memset(b, 0, sizeof b);
    put(b, 0x1F0E, "Synth4.2");
    put(b, 0x1F0E + 36, "Synth Forty Two");
    check("synthesis 4.2", b, sizeof b, "synthesis", "Synth Forty Two", "");

    memset(b, 0, sizeof b);
    put(b, 0, "FORM");
    put(b, 8, "EMODEMIC");
    put(b, 22, "Quadra Song");
    check("quadracomposer", b, 512, "quadracomposer", "Quadra Song", "");

    memset(b, 0, sizeof b);
    memcpy(b, "\0\x82\0\x48", 4);
    b[0x1F] = 0xFF;
    memcpy(b + 0x30, "\0\x01\0\x08\0\x18\0\xFF", 8);
    put(b, 8, "Laxity Title");
    check("laxity", b, 512, "laxity", "Laxity Title", "");

    memset(b, 0, sizeof b);
    put(b, 0, "PSA");
    put(b, 4, "Pro Sound Title");
    check("professionalsoundartists", b, 512, "professionalsoundartists", "Pro Sound Title", "");

    memset(b, 0, sizeof b);
    put(b, 0, "GLUE");
    memcpy(b + 4, "\xB8\xB3\xAA\xBA", 4);
    put(b, 8, "GlueSong");
    check("gluemon", b, 512, "gluemon", "GlueSong", "");

    memset(b, 0, sizeof b);
    put(b, 0, "FORM");
    b[7] = 200; /* form size */
    put(b, 8, "MODLVERS");
    b[19] = 12; /* VERS chunk size: INFO follows at 24 */
    put(b, 24, "INFO");
    b[31] = 0x34;
    put(b, 32, "PT4 Title");
    check("protracker4", b, 512, "protracker4", "PT4 Title", "");

    memset(b, 0, sizeof b);
    put(b, 0, "OBISYNTHPACK");
    check("synthpack", b, 0x510, "synthpack", "", "Karsten Obarski");

    memset(b, 0, sizeof b);
    memcpy(b, "\x60\0", 2);
    memcpy(b + 4, "\x48\xE7\xFF\xFE\x61\0", 6);
    check("wallybeben", b, 512, "wallybeben", "", "Wally Beben");

    /* bounds: a header cut before the title gives empty text; a tiny buffer truncates without overrun */
    memset(b, 0, sizeof b);
    put(b, 0, "ISM!V1.2");
    put(b, 36, "Cut");
    if (!sel_nuade_scan_text("instereo1", b, 40, t, sizeof t, a, sizeof a) || t[0] != '\0') {
        printf("FAIL short header: '%s'\n", t);
        g_fail++;
    }
    put(b, 36, "Truncated title");
    if (!sel_nuade_scan_text("instereo1", b, 512, t, 4, a, sizeof a) || strcmp(t, "Tru") != 0) {
        printf("FAIL truncation: '%s'\n", t);
        g_fail++;
    }
    if (sel_nuade_scan_text("protracker", b, 512, t, sizeof t, a, sizeof a)) {
        printf("FAIL unknown id claims a parser\n");
        g_fail++;
    }
    if (g_fail != 0) {
        printf("nuade_scan_text_fail=%d\n", g_fail);
        return 1;
    }
    printf("nuade_scan_text_ok\n");
    return 0;
}
