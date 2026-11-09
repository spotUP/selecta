/* MUI-P8 parity tests (plan 2026-10-25-amiga-mui.md section P8). Usage: mui_p8_test <name>. Host only; each test fakes the
 * engine with a send function that records the lines. */
#include <selecta/uimodel.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel/keys.h>
#include <selecta/uimodel/viewcmds.h>
#include <selecta/uimodel/panels.h>
#include <selecta/uimodel/sources.h>

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
    snprintf(cmd, sizeof cmd, "grep -rl 'desktop\\.zoom' %s/plugins/ui/mui %s/plugins/shared/mcc --include=*.c --include=*.h", MUI_SRC_ROOT, MUI_SRC_ROOT);
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

/* I1171 / I1165: the window key table (mui-p8-doors). Every multi-char action key and the page keys come from the shared tables;
 * mui_key_find is what the Selecta_Keys handler calls per key press. */
static void test_window_keys(void) {
    mui_key_bind binds[96];
    uint32_t n = mui_keys_build(binds, 96u), i, j, action = 0, door = 0, pane = 0, expect = 0, hits0 = mui_key_hits(), fired = 0;
    for (i = 0; i < n; i++) {
        expect += binds[i].shortcut == 0;
    }
    for (i = 0; i < mui_key_row_count(); i++) {
        const mui_key_row* r = mui_key_row_at(i);
        action += r->kind == MUI_KEYROW_ACTION;
        door += r->kind == MUI_KEYROW_DOOR;
        pane += r->kind == MUI_KEYROW_PANE;
        if (r->kind == MUI_KEYROW_ACTION) {
            for (j = 0; j < n; j++) {
                if (!binds[j].shortcut && strcmp(binds[j].id, r->id) == 0) {
                    CHECK(binds[j].code == r->code && binds[j].mods == r->mods, "row %s differs from mui_keys_build", r->id);
                }
            }
        }
        if (mui_key_find(r->code, r->mods, 0) == (int32_t)i) {
            fired++;
        } else {
            CHECK(mui_key_find(r->code, r->mods, 0) >= 0, "key of %s finds no row", r->id);
        }
    }
    CHECK(action == expect, "%u action rows, mui_keys_build has %u handler keys", (unsigned)action, (unsigned)expect);
    /* 28 at I1171; I1184 added M-1..M-5 (repeat off/all/one, shuffle on/off) to the shared action table: 33;
       I1186 keyed the six VIEW_STYLE rows (M-6..M-0, M-c): 39. */
    CHECK(action == 39u, "%u action rows, want 39", (unsigned)action);
    CHECK(door == 4u && pane == 2u, "%u door rows, %u pane rows, want 4 and 2", (unsigned)door, (unsigned)pane);
    CHECK(mui_key_hits() - hits0 >= action + door + pane, "hit sentinel %u", (unsigned)(mui_key_hits() - hits0));
    CHECK(fired > 0u, "no row found by its own key");
    /* A page key is text inside a text field; an action chord still fires. */
    for (i = 0; i < mui_key_row_count(); i++) {
        const mui_key_row* r = mui_key_row_at(i);
        if (r->kind != MUI_KEYROW_ACTION && r->code < 256u && r->mods == 0u) {
            CHECK(mui_key_find(r->code, r->mods, 1) < 0, "page key %s fires in a text field", r->id);
        }
    }
    {
        uint32_t base = mui_focus_routes();
        CHECK(mui_focus_route(TXT_KEY_DOWN, 0u, 1) == MUI_FOCUS_RESULTS && mui_focus_routes() == base + 1u, "Down in Search does not route");
        CHECK(mui_focus_route(TXT_KEY_DOWN, 0u, 0) == MUI_FOCUS_NONE && mui_focus_routes() == base + 1u, "Down outside Search routes");
    }
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

/* mui-p8-statistics (MUI-P8-6) */
static void test_statistics(void) {
    static const char stats[] = "Songs\0" "120\0" "Albums\0" "12\0" "Artists\0" "7\0" "Formats\0" "5\0" "Plays\0" "99\0" "Hours\0" "8\0\0";
    char less[256], more[256], none[16];
    uint32_t base = mui_stats_builds(), nl, nm, rows = sel_uim_stats_rows(stats), i, lines = 0;
    const char *l, *v;
    CHECK(rows == 6u, "%u model rows, want 6", (unsigned)rows);
    nl = mui_stats_text(stats, 0, less, sizeof less);
    nm = mui_stats_text(stats, 1, more, sizeof more);
    for (i = 0; i < nm; i++) {
        lines += more[i] == '\n';
    }
    CHECK(lines == rows, "More shows %u lines, model has %u", (unsigned)lines, (unsigned)rows);
    lines = 0;
    for (i = 0; i < nl; i++) {
        lines += less[i] == '\n';
    }
    CHECK(lines == SEL_UIM_STATS_LESS && nl < nm, "Less shows %u lines", (unsigned)lines);
    for (i = 0; i < rows; i++) {
        char want[64];
        CHECK(sel_uim_stats_row(stats, i, &l, &v) == 1u, "row %u", (unsigned)i);
        snprintf(want, sizeof want, "%s: %s\n", l, v);
        CHECK(strstr(more, want) != NULL, "panel lacks '%s'", want);
    }
    CHECK(sel_uim_stats_row(stats, rows, &l, &v) == 0u, "row past the count");
    CHECK(mui_stats_text(NULL, 1, none, sizeof none) == 0u && none[0] == '\0', "Hide/closed text not empty");
    CHECK(mui_stats_builds() - base == 3u, "sentinel %u, want 3", (unsigned)(mui_stats_builds() - base));
}

/* mui-p8-panels (MUI-P8-7) */
static void test_panels(void) {
    static mui_menu_item dummy[1];
    static mui_key_bind keys[128];
    sel_uim_net st;
    char text[512];
    uint32_t p, q, i, n, base = mui_panel_builds();
    (void)dummy;
    memset(&st, 0, sizeof st);
    st.requests = 17;
    st.zip_bytes = 4096;
    st.listing_bytes = 512;
    st.changelogs_fetched = 9;
    for (p = 0; p < MUI_PANES; p++) {
        uint32_t rows = sel_uim_panel_row_count(p), lines = 0, used = mui_panel_text(p, &st, text, sizeof text);
        CHECK(sel_uim_panel_title(p) != NULL && mui_panel_menu_id(p) != 0u, "pane %u has no title or menu id", (unsigned)p);
        for (i = 0; i < used; i++) {
            lines += text[i] == '\n';
        }
        CHECK(lines == rows && rows > 0u, "pane %u shows %u lines, model has %u", (unsigned)p, (unsigned)lines, (unsigned)rows);
        for (i = 0; i < rows; i++) {
            char row[96];
            CHECK(sel_uim_panel_row(p, &st, i, row, sizeof row) != 0u && strstr(text, row) != NULL, "pane %u lacks row %u", (unsigned)p, (unsigned)i);
        }
        for (q = p + 1u; q < MUI_PANES; q++) {
            CHECK(mui_panel_menu_id(p) != mui_panel_menu_id(q), "panes %u and %u share a menu id", (unsigned)p, (unsigned)q);
        }
    }
    mui_panel_text(SEL_UIM_PANEL_ACTIVITY, &st, text, sizeof text);
    CHECK(strstr(text, "Requests: 17") != NULL && strstr(text, "Song bytes read: 0") != NULL, "Activity lacks its rows: %s", text);
    mui_panel_text(SEL_UIM_PANEL_DOWNLOADS, &st, text, sizeof text);
    CHECK(strstr(text, "Archive bytes: 4096") != NULL, "Downloads lacks the archive bytes");
    /* the Activity key toggles the Activity pane */
    n = mui_keys_build(keys, 128);
    for (i = 0; i < n && i < 128u; i++) {
        CHECK(!(keys[i].code == 'a' && keys[i].mods == TXT_MOD_CTRL), "Ctrl+A is already bound to %s", keys[i].id);
    }
    CHECK(mui_panel_key_pane('a', TXT_MOD_CTRL) == SEL_UIM_PANEL_ACTIVITY && mui_panel_key_pane('a', 0u) == MUI_PANES, "Activity key mapping");
    CHECK(mui_panel_toggle(mui_panel_key_pane('a', TXT_MOD_CTRL)) == 1u, "first Activity key does not open");
    CHECK(mui_panel_toggle(mui_panel_key_pane('a', TXT_MOD_CTRL)) == 0u, "second Activity key does not close");
    CHECK(mui_panel_builds() - base == MUI_PANES + 2u, "sentinel %u", (unsigned)(mui_panel_builds() - base));
}

/* mui-p8-sources (MUI-P8-8) */
static void test_sources(void) {
    sel_uim_source s[3] = {{"local", "/music", 1}, {"modland", "https://m", 1}, {"hvsc", "https://h", 0}};
    fake_engine e;
    uint32_t base = mui_source_fires(), i;
    memset(&e, 0, sizeof e);
    CHECK(mui_source_toggle(s, 3, 1, fake_send, &e) == 2u, "toggle sends two lines");
    CHECK(strcmp(e.lines[0], "PREF_SET library.sources_off \"modland,hvsc\"") == 0, "toggle line: %s", e.lines[0]);
    CHECK(strcmp(e.lines[1], "SOURCES") == 0, "toggle re-reads: %s", e.lines[1]);
    memset(&e, 0, sizeof e);
    CHECK(mui_source_toggle(s, 3, 2, fake_send, &e) == 2u && strcmp(e.lines[0], "PREF_SET library.sources_off \"\"") == 0, "toggle on: %s", e.lines[0]);
    memset(&e, 0, sizeof e);
    mui_source_sync(&s[1], fake_send, &e);
    CHECK(e.n == 2u && strcmp(e.lines[0], "LIBRARY_SYNC modland \"https://m\"") == 0, "sync line: %s", e.lines[0]);
    CHECK(strcmp(mui_source_status(1, 0, 0), "Syncing") == 0 && strcmp(mui_source_status(0, 1, 5), "Sync failed") == 0 &&
              strcmp(mui_source_status(0, 0, 5), "Synced") == 0 && strcmp(mui_source_status(0, 0, 0), "Not synced") == 0,
          "status text");
    {   /* I1197: the shared SOURCES row parse and status text every UI uses */
        static const sel_tag t[] = {{SEL_SRC_KEY, (uintptr_t) "hvsc"}, {SEL_SRC_KIND, (uintptr_t) "remote"}, {SEL_SRC_NAME, (uintptr_t) "HVSC"}, {SEL_SRC_PATH, (uintptr_t) "https://h"},
                                    {SEL_SRC_SYNCABLE, 1}, {SEL_SRC_SYNCED, 1000}, {SEL_SRC_COUNT, 7}, {SEL_SRC_DUE, 1}, {SEL_TAG_END, 0}};
        static const sel_tag lt[] = {{SEL_SRC_KEY, (uintptr_t) "local"}, {SEL_SRC_KIND, (uintptr_t) "local"}, {SEL_TAG_END, 0}};
        sel_uim_src_row r, l;
        char txt[96];
        sel_uim_src_parse(t, &r);
        CHECK(strcmp(r.key, "hvsc") == 0 && strcmp(r.name, "HVSC") == 0 && r.enabled == 1 && r.syncable == 1 && r.due == 1 && r.count == 7u && r.synced == 1000 &&
                  sel_uim_src_syncable(&r) == 1,
              "parse row");
        sel_uim_src_status(&r, 0, 0, 1000 + 3 * 86400, txt, sizeof txt);
        CHECK(strcmp(txt, "Remote, last synced 3 days ago") == 0, "synced text '%s'", txt);
        sel_uim_src_status(&r, 1, 0, 0, txt, sizeof txt);
        CHECK(strcmp(txt, "Remote, Syncing") == 0, "pending text '%s'", txt);
        sel_uim_src_status(&r, 0, 1, 0, txt, sizeof txt);
        CHECK(strcmp(txt, "Remote, sync failed") == 0, "failed text '%s'", txt);
        r.enabled = 0;
        sel_uim_src_status(&r, 0, 0, 0, txt, sizeof txt);
        CHECK(strcmp(txt, "Remote, switched off") == 0, "off text '%s'", txt);
        r.enabled = 1;
        r.syncable = 0;
        sel_uim_src_status(&r, 0, 0, 0, txt, sizeof txt);
        CHECK(strcmp(txt, "Remote, search only") == 0 && sel_uim_src_syncable(&r) == 0, "search-only text '%s'", txt);
        sel_uim_src_parse(lt, &l);
        sel_uim_src_status(&l, 0, 0, 0, txt, sizeof txt);
        CHECK(l.enabled == 1 && strcmp(txt, "Local, switched on") == 0, "local text '%s'", txt);
    }
    memset(&e, 0, sizeof e);
    mui_offer_not_now(1234, fake_send, &e);
    CHECK(strcmp(e.lines[0], "PREF_SET library.sync_notice_at 1234") == 0, "Not now line: %s", e.lines[0]);
    memset(&e, 0, sizeof e);
    CHECK(mui_offer_sync(s, 3, 0x3u, fake_send, &e) == 2u && strncmp(e.lines[0], "LIBRARY_SYNC local", 18) == 0 && strncmp(e.lines[1], "LIBRARY_SYNC modland", 20) == 0,
          "offer Sync sends one per ticked source");
    CHECK(strcmp(mui_search_source_label(0), "Library") == 0 && strcmp(mui_search_source_label(1), "Web") == 0 && strcmp(mui_search_source_label(2), "Demozoo") == 0 &&
              mui_search_source_label(3) == NULL,
          "search source labels");
    for (i = 0; i < 3u; i++) {
        char want[64];
        memset(&e, 0, sizeof e);
        mui_search_source_set(i, fake_send, &e);
        snprintf(want, sizeof want, "PREF_SET search.source \"%s\"", mui_search_source_label(i));
        CHECK(e.n == 1u && strcmp(e.lines[0], want) == 0, "search source line %u: %s", (unsigned)i, e.lines[0]);
        CHECK(sel_uim_search_source_find(mui_search_source_label(i)) == i, "remembered value %u not found again", (unsigned)i);
    }
    CHECK(mui_source_fires() - base == 8u, "sentinel %u, want 8", (unsigned)(mui_source_fires() - base));
}

/* mui-p8-infoline (MUI-P8-9) */
static void test_infoline(void) {
    sel_uim_net st;
    char buf[64];
    const char keys[4] = {'7', '8', '9', '0'};
    uint32_t i;
    memset(&st, 0, sizeof st);
    for (i = 0; i < 4u; i++) {
        CHECK(mui_info_mode_key(keys[i]) == (int32_t)i, "key %c", keys[i]);
    }
    CHECK(mui_info_mode_key('1') == -1, "other key selects a mode");
    CHECK(mui_info_scroll_next(5, 20, 1) == 5u && mui_info_scroll_next(5, 20, 0) == 6u && mui_info_scroll_next(19, 20, 0) == 0u, "scroll offset");
    CHECK(strcmp(mui_info_ahi(1), "AHI") == 0 && mui_info_ahi(0)[0] == '\0', "AHI indicator");
    st.zip_bytes = 1000;
    st.stil_bytes = 24;
    CHECK(mui_info_downloaded(&st, buf, sizeof buf) != 0u && strcmp(buf, "Downloaded 1024 bytes") == 0, "byte count: %s", buf);
    CHECK(mui_list_kept_cell(1)[0] == '*' && mui_list_kept_cell(0)[0] == ' ', "kept marker");
}

/* mui-p8-tooltips (MUI-P8-10) */
static void test_tooltips(void) {
    sel_uim_frame f;
    char tip[SEL_UIM_TIP_MAX], want[SEL_UIM_TIP_MAX], status[160];
    uint32_t b, i, base = mui_tooltip_hovers(), pass;
    memset(&f, 0, sizeof f);
    for (pass = 0; pass < 2u; pass++) {
        f.play = pass != 0u ? SEL_PLAYING : SEL_STOPPED;
        for (b = 0; b < MUI_BTN_COUNT; b++) {
            const char* id = mui_transport_action(b, &f);
            uint32_t a, n = sel_uim_action_count(), ok = 0;
            CHECK(mui_tooltip_transport(b, &f, tip, sizeof tip) != 0u && tip[0] != '\0', "button %u has no help", (unsigned)b);
            for (a = 0; a < n; a++) {
                if (strcmp(sel_uim_action_at(a)->id, id) == 0) {
                    sel_uim_action_tip_text(sel_uim_action_at(a), want, sizeof want);
                    ok = strcmp(want, tip) == 0;
                }
            }
            CHECK(ok, "button %u help '%s' differs from the table", (unsigned)b, tip);
        }
    }
    for (i = 0; i < SEL_UIM_DOOR_COUNT; i++) {
        CHECK(mui_tooltip_door(i, tip, sizeof tip) != 0u && strcmp(tip, sel_uim_door_at(i)->label) == 0, "door %u help", (unsigned)i);
    }
    mui_tooltip_transport(MUI_BTN_STOP, &f, tip, sizeof tip);
    mui_tooltip_hover(tip, status, sizeof status);
    CHECK(strcmp(status, tip) == 0 && mui_tooltip_hovers() - base == 1u, "hover does not update the status line");
}

/* mui-p8-remember (MUI-P8-11) */
typedef struct fake_prefs {
    char key[4][32];
    char val[4][64];
    uint32_t n;
} fake_prefs;

static int prefs_send(void* ctx, const char* line) {
    fake_prefs* p = ctx;
    const char* k = strstr(line, "PREF_SET ");
    const char* q = k != NULL ? strchr(k + 9, ' ') : NULL;
    const char* v = q != NULL ? strchr(q, '"') : NULL;
    size_t kl, vl;
    if (k == NULL || q == NULL || v == NULL || p->n >= 4u) {
        return 0;
    }
    kl = (size_t)(q - (k + 9));
    vl = strlen(v + 1) - 1u;
    memcpy(p->key[p->n], k + 9, kl);
    memcpy(p->val[p->n], v + 1, vl);
    p->n++;
    return 1;
}

static const char* prefs_get(void* ctx, const char* key) {
    fake_prefs* p = ctx;
    uint32_t i;
    for (i = 0; i < p->n; i++) {
        if (strcmp(p->key[i], key) == 0) {
            return p->val[i];
        }
    }
    return NULL;
}

static void test_remember(void) {
    fake_prefs prefs;
    mui_remember a, b;
    memset(&prefs, 0, sizeof prefs);
    memset(&a, 0, sizeof a);
    a.x = -20;
    a.y = 15;
    a.w = 640;
    a.h = 256;
    a.panes = 5;
    snprintf(a.viz, sizeof a.viz, "viz_scope");
    snprintf(a.browse, sizeof a.browse, "columns");
    CHECK(mui_remember_open(&b, prefs_get, &prefs) == 0u, "values found before anything was stored");
    CHECK(mui_remember_close(&a, prefs_send, &prefs) == 4u, "close stores four items");
    CHECK(mui_remember_open(&b, prefs_get, &prefs) == 4u, "open finds four items");
    CHECK(b.x == -20 && b.y == 15 && b.w == 640u && b.h == 256u, "geometry %d %d %u %u", (int)b.x, (int)b.y, (unsigned)b.w, (unsigned)b.h);
    CHECK(b.panes == 5u && strcmp(b.viz, "viz_scope") == 0 && strcmp(b.browse, "columns") == 0, "panes/viz/browse: %u %s %s", (unsigned)b.panes, b.viz, b.browse);
    /* SID PatternScope (I434j4): the shared pref value round trips. */
    memset(&prefs, 0, sizeof prefs);
    snprintf(a.viz, sizeof a.viz, "viz_sidregs");
    CHECK(mui_remember_close(&a, prefs_send, &prefs) == 4u, "close stores four items (sidregs)");
    CHECK(mui_remember_open(&b, prefs_get, &prefs) == 4u && strcmp(b.viz, "viz_sidregs") == 0, "viz_sidregs round trip: %s", b.viz);
}

/* Every PREF_SET line a P8 mapping sends names a key of prefs.def, or the engine rejects it (I643). */
static int pref_known(const char* line) {
    char key[64];
    if (sscanf(line, "PREF_SET %63s", key) != 1) {
        return 0;
    }
#define SEL_PREF(id, pkey, def, kind) if (strcmp(pkey, key) == 0) return 1;
#include <selecta/prefs.def>
#undef SEL_PREF
    return 0;
}

/* I1173: Full, Window, Mini, Visual. One path: the action (menu item or its Ctrl-1..4 key row) sends PREF_SET ui.desktop.mode <name>;
 * the engine answers SEL_EVT_PREFS_CHANGED (key, value); mui_on_event hands it to mui_mode_from_pref; the map under that mode is what
 * mui_apply_layout shows. mui_mode_applies() is the sentinel that the mode code ran. */
static void test_layout_modes(void) {
    static const char* const ids[4] = {"layout_full", "layout_window", "layout_mini", "layout_visual"};
    static const char* const names[4] = {"full", "window", "mini", "visual"};
    uint32_t i, base = mui_mode_applies(), flips = 0, dummy;
    mui_layout_map_t m;
    for (i = 0; i < 4u; i++) {
        fake_engine e;
        char key[40], val[16];
        uint32_t mode = 99, j, found = 0;
        memset(&e, 0, sizeof e);
        CHECK(mui_action_fire(ids[i], fake_send, &e) == 1 && e.n == 1u, "%s not sent", ids[i]);
        CHECK(sscanf(e.lines[0], "PREF_SET %39s %15s", key, val) == 2 && strcmp(val, names[i]) == 0, "%s sent '%s'", ids[i], e.lines[0]);
        CHECK(mui_mode_from_pref(key, val, &mode) == 1 && mode == i, "%s: mode %u", ids[i], (unsigned)mode);
        for (j = 0; j < mui_key_row_count(); j++) {
            found += mui_key_row_at(j)->kind == MUI_KEYROW_ACTION && strcmp(mui_key_row_at(j)->id, ids[i]) == 0;
        }
        CHECK(found == 1u, "%s: %u key rows", ids[i], (unsigned)found);
        mui_layout_map_mode(&m, 1280, 720, SEL_UIM_TIER_DEFAULT, 0, mode);
        CHECK(m.mode == mode, "map mode");
        if (mode == SEL_UIM_LAYOUT_FULL) {
            CHECK(m.show[MUI_SLOT_SIDEBAR] && m.show[MUI_SLOT_MAIN] && m.show[MUI_SLOT_PLAYER], "Full shows all regions");
        } else if (mode == SEL_UIM_LAYOUT_WINDOW) {
            CHECK(m.show[MUI_SLOT_SIDEBAR] && m.show[MUI_SLOT_MAIN] && !m.show[MUI_SLOT_DETAIL] && m.show[MUI_SLOT_PLAYER],
                  "Window drops Now Playing only");
        } else if (mode == SEL_UIM_LAYOUT_MINI) {
            CHECK(!m.show[MUI_SLOT_SIDEBAR] && !m.show[MUI_SLOT_MAIN] && !m.show[MUI_SLOT_DETAIL] && !m.show[MUI_SLOT_TOPBAR] &&
                      m.show[MUI_SLOT_PLAYER],
                  "Mini shows the player only");
        } else {
            CHECK(!m.show[MUI_SLOT_SIDEBAR] && m.show[MUI_SLOT_MAIN] && !m.show[MUI_SLOT_DETAIL] && m.show[MUI_SLOT_PLAYER],
                  "Visual shows main and player");
        }
        mui_layout_relayout(&m, 1280, 720, SEL_UIM_TIER_DEFAULT, &flips);
        CHECK(m.mode == mode, "relayout keeps mode %s", names[i]);
        /* I1166b + I1173 merge: Expand Library and the Now Playing toggle rebuild the map; neither may reset the mode. */
        mui_side_wide_toggle(&m);
        CHECK(m.mode == mode && m.side_wide, "Expand Library keeps mode %s", names[i]);
        mui_np_toggle(&m);
        CHECK(m.mode == mode && m.side_wide, "Now Playing toggle keeps mode %s and Expand Library", names[i]);
        mui_layout_relayout(&m, 1024, 600, SEL_UIM_TIER_DEFAULT, &flips);
        CHECK(m.mode == mode && m.side_wide, "relayout keeps mode %s and Expand Library", names[i]);
    }
    CHECK(!mui_mode_from_pref("ui.desktop.viz", "visual", &dummy) && !mui_mode_from_pref("ui.desktop.mode", "lowered", &dummy),
          "foreign pref or value accepted");
    CHECK(mui_mode_applies() - base >= 3u, "sentinel: %u maps under a non-Full mode", (unsigned)(mui_mode_applies() - base));
}

/* I1174: Grid, Cover Flow, Compact. One path: the style action (menu item or key row) sends VIEW_STYLE <name>; the engine view then
 * carries that style and mui_style_pack turns the window into the List rows mui_refresh shows. The tile step numbers are the desktop's
 * (sel_uim_tile_step). mui_style_packs() is the sentinel that the packing ran. */
static void test_library_styles(void) {
    static const char* const ids[6] = {"style_regular", "style_tree", "style_drilldown", "style_grid", "style_coverflow", "style_compact"};
    static const char* const names[6] = {"regular", "tree", "drilldown", "grid", "coverflow", "compact"};
    mui_list_entry in[12], out[12];
    uint32_t i, base = mui_style_packs(), step, n;
    memset(in, 0, sizeof in);
    for (i = 0; i < 12u; i++) {
        in[i].index = 100u + i;
        snprintf(in[i].label, sizeof in[i].label, "Song %u", (unsigned)i);
        snprintf(in[i].text, sizeof in[i].text, "  Song %u (1)", (unsigned)i);
    }
    for (i = 0; i < 6u; i++) {
        fake_engine e;
        char want[40];
        memset(&e, 0, sizeof e);
        CHECK(mui_action_fire(ids[i], fake_send, &e) == 1 && e.n == 1u, "%s not sent", ids[i]);
        snprintf(want, sizeof want, "VIEW_STYLE %s", names[i]);
        CHECK(strcmp(e.lines[0], want) == 0, "%s sent '%s'", ids[i], e.lines[0]);
    }
    /* the planar tier has no Cover Flow (written exemption: the art strip needs the RTG chunky canvas); Grid and Compact are offered on both */
    CHECK((mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_GRID)) && (mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COMPACT)) &&
              !(mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COVERFLOW)) && (mui_style_mask(1) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COVERFLOW)),
          "tier style masks");
    CHECK(mui_style_kind(SEL_VIEW_STYLE_REGULAR) == MUI_STYLE_LIST && mui_style_kind(SEL_VIEW_STYLE_TREE) == MUI_STYLE_LIST &&
              mui_style_kind(SEL_VIEW_STYLE_DRILLDOWN) == MUI_STYLE_LIST && mui_style_kind(SEL_VIEW_STYLE_GRID) == MUI_STYLE_GRID &&
              mui_style_kind(SEL_VIEW_STYLE_COVERFLOW) == MUI_STYLE_COVERFLOW && mui_style_kind(SEL_VIEW_STYLE_COMPACT) == MUI_STYLE_COMPACT,
          "style kinds");
    CHECK(mui_style_pack(SEL_VIEW_STYLE_REGULAR, 3, 12, in, 12, 0, out, 12) == 0u && mui_style_packs() == base, "list style packed");
    /* tile steps: cell width follows the desktop's step px, columns shrink as the step grows, the step clamps */
    for (step = 0; step < SEL_UIM_TILE_STEPS; step++) {
        CHECK(mui_style_cell_chars(step) == (uint32_t)(sel_uim_tile_step(step) / 8), "cell chars step %u", (unsigned)step);
    }
    CHECK(mui_style_grid_cols(80, 0) > mui_style_grid_cols(80, SEL_UIM_TILE_STEPS - 1u) && mui_style_grid_cols(4, 4) == 1u, "cols by step");
    CHECK(mui_tile_step_move(2, 1) == 3u && mui_tile_step_move(2, -1) == 1u && mui_tile_step_move(4, 1) == 4u && mui_tile_step_move(0, -1) == 0u,
          "step move clamp");
    /* Grid: 12 cells in rows of 5 -> 3 rows; the cursor cell is marked; a click maps to the first cell, or keeps the cursor in its row */
    n = mui_style_pack(SEL_VIEW_STYLE_GRID, 5, 12, in, 12, 7, out, 12);
    CHECK(n == 3u && out[0].index == 100u && out[1].index == 105u && out[2].index == 110u, "grid rows %u", (unsigned)n);
    CHECK(strstr(out[1].text, "[Song 7") != NULL && strstr(out[0].text, "[") == NULL && strstr(out[0].text, "Song 4") != NULL, "grid text '%s'", out[1].text);
    CHECK(mui_style_row_index(SEL_VIEW_STYLE_GRID, 5, 2, 7) == 10 && mui_style_row_index(SEL_VIEW_STYLE_GRID, 5, 1, 7) == 7, "grid click map");
    CHECK(mui_style_packed_cursor(SEL_VIEW_STYLE_GRID, 5, 7) == 1 && mui_style_packed_cursor(SEL_VIEW_STYLE_GRID, 5, -1) == -1, "grid cursor row");
    /* Cover Flow: one row, the cursor between two neighbours each side */
    n = mui_style_pack(SEL_VIEW_STYLE_COVERFLOW, 1, 14, in, 12, 5, out, 12);
    CHECK(n == 1u && out[0].index == 105u && strstr(out[0].text, "[Song 5") != NULL && strstr(out[0].text, "Song 3") != NULL &&
              strstr(out[0].text, "Song 7") != NULL && strstr(out[0].text, "Song 8") == NULL, "coverflow '%s'", out[0].text);
    CHECK(mui_style_row_index(SEL_VIEW_STYLE_COVERFLOW, 1, 0, 5) == 5, "coverflow click");
    /* Compact: one dense label per row, no indent or count */
    n = mui_style_pack(SEL_VIEW_STYLE_COMPACT, 1, 12, in, 12, 0, out, 12);
    CHECK(n == 12u && strcmp(out[3].text, "Song 3") == 0, "compact '%s'", out[3].text);
    CHECK(mui_style_packs() - base == 3u, "sentinel: %u packs, want 3", (unsigned)(mui_style_packs() - base));
}

static void test_pref_keys(void) {
    char o[SEL_UIM_CMD_MAX];
    uint32_t i;
    for (i = 0; i < SEL_UIM_REM_COUNT; i++) {
        CHECK(sel_uim_remember_line(i, "x", o, sizeof o) != 0u && pref_known(o), "remembered item %u: '%s'", (unsigned)i, o);
    }
    CHECK(sel_uim_search_source_line(0u, o, sizeof o) != 0u && pref_known(o), "search source: '%s'", o);
    CHECK(sel_uim_hotkeys_line(1, o, sizeof o) != 0u && pref_known(o), "hotkeys: '%s'", o);
    CHECK(sel_uim_priority_line(2, o, sizeof o) != 0u && pref_known(o), "task priority: '%s'", o);
    CHECK(sel_uim_groupfile_line("S:Selecta.group", o, sizeof o) != 0u && pref_known(o), "group file: '%s'", o);
    CHECK(sel_uim_sources_off_line(NULL, 0u, 0u, 0u, o, sizeof o) != 0u && pref_known(o), "sources off: '%s'", o);
    CHECK(sel_uim_sync_notnow_line(1, o, sizeof o) != 0u && pref_known(o), "not now: '%s'", o);
}

/* mui-p8-queue-scope-ui (I1176): the Queue scope Cycle in mui_ui.c is wired to the shared scope action. The Cycle entries are the
 * desktop labels (sel_uim_scope_label), a pick goes through mui_scope_fire (SCOPE <name> + queue re-read), and the Cycle follows the
 * engine's SEL_OPT_SCOPE through sel_uim_scope_index. The sentinel counts the real fire path. */
static int src_count(const char* pattern) {
    char path[512], cmd[1024], buf[64];
    FILE* f;
    int n = -1;
    snprintf(path, sizeof path, "%s/plugins/ui/mui/mui_ui.c", MUI_SRC_ROOT);
    snprintf(cmd, sizeof cmd, "grep -c -F '%s' %s", pattern, path);
    f = popen(cmd, "r");
    if (f != NULL) {
        if (fgets(buf, sizeof buf, f) != NULL) {
            n = atoi(buf);
        }
        pclose(f);
    }
    return n;
}

static void test_queue_scope_ui(void) {
    static const char* const label[MUI_SCOPE_COUNT] = {"Scope: Library", "Scope: Album", "Scope: Playlist"};
    static const char* const word[MUI_SCOPE_COUNT] = {"library", "album", "playlist"};
    uint32_t i, base = mui_view_fires();
    for (i = 0; i < MUI_SCOPE_COUNT; i++) {
        fake_engine e;
        char line[32];
        CHECK(sel_uim_scope_label(i) != NULL && strcmp(sel_uim_scope_label(i), label[i]) == 0, "scope %u label", (unsigned)i);
        snprintf(line, sizeof line, "SCOPE %s", word[i]);
        memset(&e, 0, sizeof e);
        CHECK(mui_scope_fire(i, fake_send, &e) == 2u && strcmp(e.lines[0], line) == 0, "scope %u action: '%s'", (unsigned)i, e.lines[0]);
        CHECK(sel_uim_scope_index(word[i]) == i, "scope %u read back", (unsigned)i);
    }
    CHECK(mui_view_fires() - base == MUI_SCOPE_COUNT, "sentinel: %u fires", (unsigned)(mui_view_fires() - base));
    CHECK(src_count("mui_scope_fire((uint32_t)act, mui_p8_send, s)") == 1, "CTL_SCOPE is not wired to mui_scope_fire in mui_ui.c");
    CHECK(src_count("sel_uim_scope_index(st->options[SEL_OPT_SCOPE])") == 1, "the scope Cycle does not follow SEL_OPT_SCOPE");
}

/* mui-p8-library-pages (I1175): Files, History, Sources modes and the Sources page rows, switch and Sync, fed with real engine events */
static sel_message src_event(const sel_tag* t) {
    sel_message m;
    memset(&m, 0, sizeof m);
    m.kind = SEL_MSG_EVT;
    m.id = SEL_EVT_LIBRARY_RESULT;
    m.argc = 1;
    m.args[0].type = SEL_ARG_TAGS;
    m.args[0].v.t = t;
    return m;
}

static void test_library_pages(void) {
    static const sel_tag loc[] = {{SEL_SRC_KEY, (uintptr_t) "local"}, {SEL_SRC_KIND, (uintptr_t) "local"}, {SEL_SRC_NAME, (uintptr_t) "Library"}, {SEL_SRC_PATH, (uintptr_t) "/music"}, {SEL_SRC_ENABLED, 1}, {SEL_TAG_END, 0}};
    static const sel_tag rem[] = {{SEL_SRC_KEY, (uintptr_t) "modland"}, {SEL_SRC_KIND, (uintptr_t) "remote"}, {SEL_SRC_NAME, (uintptr_t) "Modland"}, {SEL_SRC_PATH, (uintptr_t) "https://m"},
                                  {SEL_SRC_ENABLED, 1}, {SEL_SRC_SYNCABLE, 1}, {SEL_TAG_END, 0}};
    static const sel_tag done[] = {{SEL_SRC_PATH, (uintptr_t) "https://m"}, {SEL_TAG_END, 0}};
    static mui_srcs m;
    sel_message ev;
    fake_engine e;
    char row[128], joined[256];
    uint32_t vbase = mui_view_fires(), sbase = mui_source_fires();
    memset(&m, 0, sizeof m);
    CHECK(mui_libmode_joined(3, 12, joined, sizeof joined) == 3u && strcmp(joined, "VIEW_STYLE regular;VIEW_SOURCE browse;VIEW_WINDOW 12") == 0, "Files modes: '%s'", joined);
    CHECK(mui_libmode_joined(4, 12, joined, sizeof joined) == 3u && strcmp(joined, "VIEW_STYLE regular;VIEW_SOURCE home;VIEW_WINDOW 12") == 0, "History modes: '%s'", joined);
    CHECK(mui_libmode_joined(5, 12, joined, sizeof joined) >= 1u && strncmp(joined, "SOURCES", 7) == 0, "Sources mode: '%s'", joined);
    mui_srcs_asked(&m);
    ev = src_event(loc);
    CHECK(mui_srcs_event(&m, &ev) == 1, "local row not taken");
    ev = src_event(rem);
    CHECK(mui_srcs_event(&m, &ev) == 1 && m.n == 2u, "remote row not taken: %u rows", (unsigned)m.n);
    mui_srcs_row(&m, 1, row, sizeof row);
    CHECK(strstr(row, "Modland") != NULL && strstr(row, "Remote, never synced") != NULL, "remote row text '%s'", row);
    mui_srcs_row(&m, 0, row, sizeof row);
    CHECK(strstr(row, "Library") != NULL && strstr(row, "Local, switched on") != NULL, "local row text '%s'", row);
    memset(&e, 0, sizeof e);
    CHECK(mui_srcs_sync(&m, 0, fake_send, &e) == 0u && e.n == 0u, "local row synced");
    CHECK(mui_srcs_sync(&m, 1, fake_send, &e) == 2u && strcmp(e.lines[0], "LIBRARY_SYNC modland \"https://m\"") == 0 && strcmp(e.lines[1], "SOURCES") == 0, "Sync lines '%s'", e.lines[0]);
    mui_srcs_row(&m, 1, row, sizeof row);
    CHECK(strstr(row, "Syncing") != NULL, "pending row text '%s'", row);
    ev = src_event(done);
    CHECK(mui_srcs_event(&m, &ev) == 0 && m.pending == 0u, "completion left the row pending");
    mui_srcs_row(&m, 1, row, sizeof row);
    CHECK(strstr(row, "Remote, never synced") != NULL, "settled row text '%s'", row);
    memset(&ev, 0, sizeof ev);
    ev.kind = SEL_MSG_EVT;
    ev.id = SEL_EVT_ERROR;
    ev.argc = 2;
    ev.args[0].type = SEL_ARG_STR;
    ev.args[0].v.s = "sync_failed";
    ev.args[1].type = SEL_ARG_STR;
    ev.args[1].v.s = "https://m";
    mui_srcs_event(&m, &ev);
    CHECK(m.failed == 0u, "an error with nothing pending marked failed");
    memset(&e, 0, sizeof e);
    mui_srcs_sync(&m, 1, fake_send, &e);
    mui_srcs_event(&m, &ev);
    mui_srcs_row(&m, 1, row, sizeof row);
    CHECK(m.pending == 0u && strstr(row, "Remote, sync failed") != NULL, "failed row text '%s'", row);
    memset(&e, 0, sizeof e);
    CHECK(mui_srcs_toggle(&m, 1, fake_send, &e) == 2u && strcmp(e.lines[0], "PREF_SET library.sources_off \"modland\"") == 0 && strcmp(e.lines[1], "SOURCES") == 0, "toggle lines '%s'", e.lines[0]);
    CHECK(mui_srcs_toggle(&m, 9, fake_send, &e) == 0u, "toggle of a missing row sent");
    CHECK(mui_view_fires() - vbase == 3u && mui_source_fires() - sbase == 3u, "sentinels view %u source %u", (unsigned)(mui_view_fires() - vbase), (unsigned)(mui_source_fires() - sbase));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    if (strcmp(argv[1], "library-pages") == 0) {
        test_library_pages();
    } else if (strcmp(argv[1], "panels") == 0) {
        test_panels();
    } else if (strcmp(argv[1], "sources") == 0) {
        test_sources();
    } else if (strcmp(argv[1], "infoline") == 0) {
        test_infoline();
    } else if (strcmp(argv[1], "tooltips") == 0) {
        test_tooltips();
    } else if (strcmp(argv[1], "remember") == 0) {
        test_remember();
    } else if (strcmp(argv[1], "transport-extras") == 0) {
        test_transport_extras();
    } else if (strcmp(argv[1], "voices-programs") == 0) {
        test_voices_programs();
    } else if (strcmp(argv[1], "library-modes") == 0) {
        test_library_modes();
    } else if (strcmp(argv[1], "queue-scope-ui") == 0) {
        test_queue_scope_ui();
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
        test_window_keys();
    } else if (strcmp(argv[1], "statistics") == 0) {
        test_statistics();
    } else if (strcmp(argv[1], "layout-modes") == 0) {
        test_layout_modes();
    } else if (strcmp(argv[1], "library-styles") == 0) {
        test_library_styles();
    } else if (strcmp(argv[1], "pref-keys") == 0) {
        test_pref_keys();
    } else {
        printf("unknown test %s\n", argv[1]);
        return 2;
    }
    if (failures == 0) {
        printf("PASS %s\n", argv[1]);
    }
    return failures != 0;
}
