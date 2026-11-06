/* I710ga6: the Now Playing note line and its button for a song that waits for a required system file (plan section 7).
 * Where the behaviour lives: sel_uim_np_note / sel_uim_np_note_action / sel_uim_sysfile_text. */
#include <selecta/engine.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); return 1; } } while (0)

int main(void) {
    sel_song_state s;
    char text[256];
    memset(&s, 0, sizeof s);
    CHECK(sel_uim_np_note(&s) == NULL && sel_uim_np_note_action(&s) == NULL, "no wait, no note");
    s.sysfile_label = "Test system file";
    s.sysfile_state = SEL_SYSF_ST_FETCHING;
    CHECK(sel_uim_np_note(&s) == NULL && sel_uim_np_note_action(&s) == NULL, "fetching shows nothing (Q2)");
    s.sysfile_state = SEL_SYSF_ST_FAILED;
    CHECK(sel_uim_np_note(&s) != NULL && strcmp(sel_uim_np_note(&s), "Test system file needed") == 0, "note '%s'", sel_uim_np_note(&s));
    CHECK(sel_uim_np_note_action(&s) != NULL && strcmp(sel_uim_np_note_action(&s), "Get File...") == 0, "button");
    s.path = "/x.mod";
    s.bank_missing = 1u;
    CHECK(strcmp(sel_uim_np_note(&s), "Sample bank not found") == 0, "the bank note keeps its place");
    sel_uim_sysfile_text("Test system file", "all_urls_failed", text, sizeof text);
    CHECK(strstr(text, "Test system file") != NULL && strstr(text, "Every download address failed.") != NULL, "text '%s'", text);
    sel_uim_sysfile_text("Test system file", "no_match", text, sizeof text);
    CHECK(strstr(text, "No download address matched.") != NULL, "text '%s'", text);
    printf("sysfile_note_ok\n");
    return 0;
}
