/* MUI-P8 parity tests (plan 2026-10-25-amiga-mui.md section P8). Usage: mui_p8_test <name>. Host only; each test fakes the
 * engine with a send function that records the lines. */
#include <selecta/uimodel.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/keys.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

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

typedef struct fake_engine {
    char lines[32][64];
    uint32_t n;
} fake_engine;

static int fake_send(void* ctx, const char* line) {
    fake_engine* e = ctx;
    if (e->n < 32u) {
        snprintf(e->lines[e->n], sizeof e->lines[0], "%s", line);
    }
    e->n++;
    return 1;
}

/* mui-p8-transport-extras (MUI-P8-1) */
static void test_transport_extras(void) {
    static const char* const want_line[MUI_EXTRA_COUNT] = {"NEXT_ALBUM", "PREV_ALBUM", "PITCH +100", "PITCH -100", "TEMPO +5", "TEMPO -5"};
    static mui_menu_item items[512];
    static mui_key_bind keys[128];
    fake_engine e;
    uint32_t i, m, k, menu_n, key_n, base = mui_action_fires();
    memset(&e, 0, sizeof e);
    menu_n = mui_menu_build(items, 512);
    key_n = mui_keys_build(keys, 128);
    CHECK(mui_extra_action(MUI_EXTRA_COUNT) == NULL, "extra past the count");
    for (i = 0; i < MUI_EXTRA_COUNT; i++) {
        const char* id = mui_extra_action(i);
        uint32_t in_menu = 0, in_keys = 0;
        CHECK(id != NULL, "extra %u has no action", (unsigned)i);
        for (m = 0; id != NULL && m < menu_n; m++) {
            in_menu += strcmp(items[m].id, id) == 0;
        }
        for (k = 0; id != NULL && k < key_n; k++) {
            in_keys += strcmp(keys[k].id, id) == 0;
        }
        CHECK(in_menu == 1u, "%s: %u menu items", id, (unsigned)in_menu);
        CHECK(in_keys == 1u, "%s: %u key bindings", id, (unsigned)in_keys);
        CHECK(mui_extra_label(i) != NULL && mui_extra_label(i)[0] != '\0', "%s: no label", id);
        CHECK(mui_action_fire(id, fake_send, &e) == 1u, "%s: not sent", id);
    }
    CHECK(e.n == MUI_EXTRA_COUNT, "engine got %u lines, want exactly %u", (unsigned)e.n, (unsigned)MUI_EXTRA_COUNT);
    for (i = 0; i < MUI_EXTRA_COUNT && i < e.n; i++) {
        CHECK(strcmp(e.lines[i], want_line[i]) == 0, "line %u is '%s', want '%s'", (unsigned)i, e.lines[i], want_line[i]);
    }
    CHECK(mui_action_fires() - base == 6u, "handler sentinel %u, want 6", (unsigned)(mui_action_fires() - base));
}

/* mui-p8-voices-programs (MUI-P8-2) */
static void test_voices_programs(void) {
    static mui_menu_item items[512];
    static mui_key_bind keys[128];
    static const char* const ids[2] = {"solo_clear", "filter_led"};
    static const char* const want[2] = {"SOLO 0", "FILTER_LED toggle"};
    mui_fkey_bind fk[32];
    fake_engine e;
    uint32_t i, m, n, menu_n = mui_menu_build(items, 512), key_n = mui_keys_build(keys, 128), base = mui_action_fires();
    memset(&e, 0, sizeof e);
    for (i = 0; i < 2; i++) {
        uint32_t in_menu = 0, in_keys = 0;
        for (m = 0; m < menu_n; m++) {
            in_menu += strcmp(items[m].id, ids[i]) == 0;
        }
        for (m = 0; m < key_n; m++) {
            in_keys += strcmp(keys[m].id, ids[i]) == 0;
        }
        CHECK(in_menu == 1u && in_keys == 1u, "%s: %u menu items, %u keys", ids[i], (unsigned)in_menu, (unsigned)in_keys);
        CHECK(mui_action_fire(ids[i], fake_send, &e) == 1u, "%s not sent", ids[i]);
    }
    CHECK(e.n == 2u && strcmp(e.lines[0], want[0]) == 0 && strcmp(e.lines[1], want[1]) == 0, "engine got %u lines: '%s' '%s'", (unsigned)e.n, e.lines[0], e.lines[1]);
    CHECK(mui_action_fires() - base == 2u, "sentinel");
    n = mui_fkeys_build(fk, 32);
    CHECK(n == 20u, "%u program keys, want 20", (unsigned)n);
    for (i = 0; i < n && i < 32u; i++) {
        uint32_t j;
        for (j = 0; j < i; j++) {
            CHECK(fk[i].code != fk[j].code || fk[i].mods != fk[j].mods, "key %u repeats key %u", (unsigned)i, (unsigned)j);
        }
    }
    for (i = 1; i <= 10u; i++) {
        char want_load[32], want_save[32];
        snprintf(want_load, sizeof want_load, "PLAYLIST_LOAD F%u", (unsigned)i);
        snprintf(want_save, sizeof want_save, "PLAYLIST_SAVE F%u", (unsigned)i);
        memset(&e, 0, sizeof e);
        CHECK(mui_fkey_fire(TXT_KEY_F1 + i - 1u, 0, fake_send, &e) == 2u && strcmp(e.lines[0], want_load) == 0 && strcmp(e.lines[1], "PLAY") == 0, "F%u recall: %u lines '%s'", (unsigned)i, (unsigned)e.n, e.lines[0]);
        memset(&e, 0, sizeof e);
        CHECK(mui_fkey_fire(TXT_KEY_F1 + i - 1u, TXT_MOD_SHIFT, fake_send, &e) == 1u && strcmp(e.lines[0], want_save) == 0, "Shift-F%u store: %u lines '%s'", (unsigned)i, (unsigned)e.n, e.lines[0]);
    }
    memset(&e, 0, sizeof e);
    CHECK(mui_fkey_fire(TXT_KEY_F1 + 10u, 0, fake_send, &e) == 0u && mui_fkey_fire('a', 0, fake_send, &e) == 0u && e.n == 0u, "a non-program key sent a line");
}

/* mui-p8-library-modes (MUI-P8-3) */
static void test_library_modes(void) {
    static const char* const want_name[MUI_LIBMODE_COUNT] = {"Composers", "Tree", "Columns", "Files", "History", "Sources"};
    static const char* const want_first[MUI_LIBMODE_COUNT] = {"VIEW_STYLE regular", "LIBRARY_SEARCH \"\"", "LIBRARY_SEARCH \"\"", "VIEW_STYLE regular", "VIEW_STYLE regular", "SOURCES"};
    static const char* const layouts[4] = {"layout_full", "layout_window", "layout_mini", "layout_visual"};
    static mui_menu_item items[512];
    fake_engine e;
    uint32_t i, m, menu_n = mui_menu_build(items, 512), base = mui_view_fires(), abase = mui_action_fires();
    CHECK(mui_libmode_label(MUI_LIBMODE_COUNT) == NULL, "mode past the count");
    for (i = 0; i < MUI_LIBMODE_COUNT; i++) {
        CHECK(mui_libmode_label(i) != NULL && strcmp(mui_libmode_label(i), want_name[i]) == 0, "mode %u label", (unsigned)i);
        memset(&e, 0, sizeof e);
        CHECK(mui_libmode_fire(i, 12, fake_send, &e) == e.n && e.n >= 1u, "mode %u: %u lines", (unsigned)i, (unsigned)e.n);
        CHECK(strcmp(e.lines[0], want_first[i]) == 0, "mode %u first line '%s'", (unsigned)i, e.lines[0]);
    }
    memset(&e, 0, sizeof e);
    mui_libmode_fire(4, 12, fake_send, &e);
    CHECK(e.n == 3u && strcmp(e.lines[1], "VIEW_SOURCE home") == 0 && strcmp(e.lines[2], "VIEW_WINDOW 12") == 0, "History lines");
    CHECK(mui_view_fires() - base == 7u, "sentinel %u, want 7", (unsigned)(mui_view_fires() - base));
    for (i = 0; i < 4; i++) {
        uint32_t in_menu = 0;
        for (m = 0; m < menu_n; m++) {
            in_menu += strcmp(items[m].id, layouts[i]) == 0;
        }
        CHECK(in_menu == 1u, "%s: %u menu items", layouts[i], (unsigned)in_menu);
    }
    memset(&e, 0, sizeof e);
    CHECK(mui_action_fire("layout_visual", fake_send, &e) == 1u && e.n == 1u && mui_action_fires() - abase == 1u, "Visual Layout not sent");
}

/* mui-p8-queue-scope (MUI-P8-4) */
static void test_queue_scope(void) {
    static const char* const want[MUI_SCOPE_COUNT] = {"library", "album", "playlist"};
    fake_engine e;
    uint32_t i, base = mui_view_fires();
    CHECK(mui_scope_label(MUI_SCOPE_COUNT) == NULL, "scope past the count");
    for (i = 0; i < MUI_SCOPE_COUNT; i++) {
        char line[32];
        snprintf(line, sizeof line, "SCOPE %s", want[i]);
        CHECK(mui_scope_label(i) != NULL && strcmp(mui_scope_label(i), want[i]) == 0, "scope %u label", (unsigned)i);
        memset(&e, 0, sizeof e);
        CHECK(mui_scope_fire(i, fake_send, &e) == 2u && strcmp(e.lines[0], line) == 0 && strcmp(e.lines[1], "VIEW_SOURCE queue") == 0, "scope %u: '%s' %u lines", (unsigned)i, e.lines[0], (unsigned)e.n);
    }
    memset(&e, 0, sizeof e);
    CHECK(mui_scope_fire(MUI_SCOPE_COUNT, fake_send, &e) == 0u && e.n == 0u, "a bad scope sent a line");
    CHECK(mui_view_fires() - base == 4u, "sentinel");
}

/* mui-p8-view-buttons (MUI-P8-5) */
static void test_view_buttons(void) {
    static const char* const want_label[MUI_VIEWBTN_COUNT] = {"New Folder", "New Playlist", "Mark Wrong", "Skip Song", "Save Note"};
    static const char* const want_line[MUI_VIEWBTN_COUNT] = {"FOLDER_ADD \"Mixes\" 3", "PLAYLIST_CREATE \"Mixes\" 3", "FLAG wrong \"Mixes\"", "FLAG skip", "NOTE \"Mixes\""};
    fake_engine e;
    uint32_t i, base = mui_view_fires();
    CHECK(mui_viewbtn_label(MUI_VIEWBTN_COUNT) == NULL, "button past the count");
    for (i = 0; i < MUI_VIEWBTN_COUNT; i++) {
        CHECK(mui_viewbtn_label(i) != NULL && strcmp(mui_viewbtn_label(i), want_label[i]) == 0, "button %u label", (unsigned)i);
        memset(&e, 0, sizeof e);
        CHECK(mui_viewbtn_fire(i, "Mixes", 3, fake_send, &e) == 1u && e.n == 1u && strcmp(e.lines[0], want_line[i]) == 0, "button %u: %u lines '%s'", (unsigned)i, (unsigned)e.n, e.lines[0]);
    }
    memset(&e, 0, sizeof e);
    mui_viewbtn_fire(3, "", 0, fake_send, &e);
    mui_viewbtn_fire(2, "", 0, fake_send, &e);
    mui_viewbtn_fire(4, NULL, 0, fake_send, &e);
    CHECK(e.n == 3u && strcmp(e.lines[0], "FLAG skip") == 0 && strcmp(e.lines[1], "FLAG wrong") == 0 && strcmp(e.lines[2], "NOTE") == 0, "empty note lines");
    memset(&e, 0, sizeof e);
    CHECK(mui_viewbtn_fire(MUI_VIEWBTN_COUNT, "x", 0, fake_send, &e) == 0u && e.n == 0u, "a bad button sent a line");
    CHECK(mui_view_fires() - base == 9u, "sentinel %u, want 9", (unsigned)(mui_view_fires() - base));
}

/* mui-p8-tile-steps (MUI-P8-12): the Slider steps are the desktop's (sel_uim_tile_step, which the desktop reads too). */
static void test_tile_steps(void) {
    uint32_t i;
    char cmd[512];
    FILE* f;
    char out[256] = "";
    CHECK(mui_tile_steps() == 5u, "%u steps", (unsigned)mui_tile_steps());
    for (i = 0; i < 5u; i++) {
        CHECK(mui_tile_step(i) == sel_uim_tile_step(i) && mui_tile_step(i) > 0, "step %u", (unsigned)i);
    }
    CHECK(mui_tile_step(5) == 0, "step past the count");
    CHECK(mui_tile_step(0) < mui_tile_step(4), "steps not increasing");
    /* the written exemption: no MUI source reads ui.desktop.zoom */
    snprintf(cmd, sizeof cmd, "grep -rl 'desktop\\.zoom' %s/plugins/ui/mui --include=*.c --include=*.h", MUI_SRC_ROOT);
    f = popen(cmd, "r");
    CHECK(f != NULL, "popen");
    if (f != NULL) {
        CHECK(fgets(out, sizeof out, f) == NULL, "a MUI source reads ui.desktop.zoom: %s", out);
        pclose(f);
    }
}

/* mui-p8-amiga-settings (MUI-P8-14) */
static void test_amiga_settings(void) {
    fake_engine e;
    uint32_t base = mui_pref_sets();
    memset(&e, 0, sizeof e);
    CHECK(mui_amiga_hotkeys_set(1, fake_send, &e) == 1u, "hotkeys not sent");
    CHECK(mui_amiga_priority_set(3, fake_send, &e) == 1u, "priority not sent");
    CHECK(mui_amiga_group_set("SYS:Prefs/Hippo.group", fake_send, &e) == 1u, "group not sent");
    CHECK(e.n == 3u && strcmp(e.lines[0], "PREF_SET amiga.hotkeys 1") == 0 && strcmp(e.lines[1], "PREF_SET amiga.task_priority 3") == 0 && strcmp(e.lines[2], "PREF_SET amiga.group_file \"SYS:Prefs/Hippo.group\"") == 0, "lines: '%s' '%s' '%s'", e.lines[0], e.lines[1], e.lines[2]);
    CHECK(mui_pref_sets() - base == 3u, "pref-set count %u, want 3", (unsigned)(mui_pref_sets() - base));
    memset(&e, 0, sizeof e);
    mui_amiga_priority_set(99, fake_send, &e);
    mui_amiga_priority_set(-99, fake_send, &e);
    CHECK(e.n == 2u && strcmp(e.lines[0], "PREF_SET amiga.task_priority 5") == 0 && strcmp(e.lines[1], "PREF_SET amiga.task_priority -5") == 0, "clamp: '%s' '%s'", e.lines[0], e.lines[1]);
}

/* mui-p8-doors (MUI-P8-13) */
static void test_doors(void) {
    static const char* const want_line[7] = {"VIEW_SOURCE home", "VIEW_SOURCE search", "VIEW_SHOW browse", "VIEW_SOURCE folders", "VIEW_SHOW queue", NULL, NULL};
    static const char* const want_page[7] = {NULL, NULL, NULL, NULL, NULL, "nowplaying", "songinfo"};
    fake_engine e;
    uint32_t i, base = mui_page_switches();
    CHECK(mui_door_count() == 7u, "%u doors", (unsigned)mui_door_count());
    for (i = 0; i < 7u; i++) {
        const char* page = "x";
        uint32_t before;
        memset(&e, 0, sizeof e);
        before = e.n;
        mui_door_activate(i, fake_send, &e, &page);
        if (want_line[i] != NULL) {
            CHECK(e.n == before + 1u && strcmp(e.lines[0], want_line[i]) == 0 && page == NULL, "door %u: %u lines", (unsigned)i, (unsigned)e.n);
        } else {
            CHECK(e.n == 0u && page != NULL && strcmp(page, want_page[i]) == 0, "door %u: %u lines, page '%s'", (unsigned)i, (unsigned)e.n, page != NULL ? page : "(null)");
        }
    }
    CHECK(mui_page_switches() - base == 2u, "page-switch sentinel %u, want 2", (unsigned)(mui_page_switches() - base));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    if (strcmp(argv[1], "transport-extras") == 0) {
        test_transport_extras();
    } else if (strcmp(argv[1], "voices-programs") == 0) {
        test_voices_programs();
    } else if (strcmp(argv[1], "library-modes") == 0) {
        test_library_modes();
    } else if (strcmp(argv[1], "queue-scope") == 0) {
        test_queue_scope();
    } else if (strcmp(argv[1], "view-buttons") == 0) {
        test_view_buttons();
    } else if (strcmp(argv[1], "tile-steps") == 0) {
        test_tile_steps();
    } else if (strcmp(argv[1], "amiga-settings") == 0) {
        test_amiga_settings();
    } else if (strcmp(argv[1], "doors") == 0) {
        test_doors();
    } else {
        printf("unknown test %s\n", argv[1]);
        return 2;
    }
    if (failures == 0) {
        printf("PASS %s\n", argv[1]);
    }
    return failures != 0;
}
