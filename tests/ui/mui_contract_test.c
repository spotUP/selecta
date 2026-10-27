/* mui-wait-contract (MUI-P1-7), mui-registry (MUI-P1-6), mui-open-failure (MUI-P1-4), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_contract_test wait|registry|failure */
#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

extern const sel_tag* sel_plugin_entry_mui_test(uint32_t host_api);

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

/* Host stub of the MUI loop: scripted ReturnID, the signals MUI asks for, the signals that arrive. */
typedef struct stub {
    int id;
    uint32_t want;
    uint32_t arrive;
    uint32_t mask_seen;
    int waits;
} stub;

static int stub_input(void* ctx, uint32_t* sigs) {
    stub* s = ctx;
    *sigs = s->want;
    return s->id;
}

static uint32_t stub_wait(void* ctx, uint32_t mask, uint32_t timeout_ms) {
    stub* s = ctx;
    (void)timeout_ms;
    s->waits++;
    s->mask_seen = mask;
    return s->arrive & mask;
}

static int probe_no(void) {
    return 0;
}
static int probe_yes(void) {
    return 1;
}

static void test_wait(void) {
    mui_wait_io io = {stub_input, stub_wait, 0x1000u, 0x4000u};
    stub s = {0, 0x0100u, 0, 0, 0};
    CHECK(mui_wait_step(&io, &s, 20) == 0, "nothing happened is not 0 (timeout)");
    CHECK((s.mask_seen & 0x4000u) != 0, "engine wake signal missing from the wait mask %x", (unsigned)s.mask_seen);
    CHECK((s.mask_seen & 0x0100u) != 0 && (s.mask_seen & 0x1000u) != 0, "MUI and break signals missing %x",
          (unsigned)s.mask_seen);
    s.id = 7;
    s.waits = 0;
    CHECK(mui_wait_step(&io, &s, 20) == 1 && s.waits == 0, "a ReturnID is not 1 (handled)");
    s.id = 0;
    s.arrive = 0x4000u;
    CHECK(mui_wait_step(&io, &s, 20) == 1, "engine wake is not 1");
    s.arrive = 0x0100u;
    CHECK(mui_wait_step(&io, &s, 20) == 1, "MUI signal is not 1");
    s.arrive = 0x1000u;
    CHECK(mui_wait_step(&io, &s, 20) == -1, "break is not -1 (quit)");
    s.id = MUI_WAIT_ID_QUIT;
    s.waits = 0;
    CHECK(mui_wait_step(&io, &s, 20) == -1 && s.waits == 0, "quit ReturnID is not -1");
}

static void test_registry(void) {
    const sel_tag* d;
    mui_set_probe(probe_no);
    d = sel_plugin_entry_mui_test(1u);
    CHECK(d != NULL && strcmp((const char*)sel_tag_find(d, SEL_PLUGIN_ID, 0), "mui") == 0, "mui not registered when absent");
    CHECK(!sel_registry_ui_listed(d), "mui listed although MUI is absent");
    mui_set_probe(NULL);
    d = sel_plugin_entry_mui_test(1u);
    CHECK(d != NULL && !sel_registry_ui_listed(d), "mui listed with no probe");
    mui_set_probe(probe_yes);
    d = sel_plugin_entry_mui_test(1u);
    CHECK(d != NULL && sel_registry_ui_listed(d), "mui not listed with MUI");
}

static void test_failure(void) {
    const char* a = mui_fail_text(MUI_FAIL_NO_LIBRARY);
    const char* b = mui_fail_text(MUI_FAIL_NO_WINDOW);
    const sel_tag* hooks;
    sel_ui_open_fn open;
    CHECK(strcmp(a, b) != 0, "library and window failures share one text");
    CHECK(strstr(a, "muimaster.library") != NULL && strstr(a, "text UI") != NULL, "library text: %s", a);
    CHECK(strstr(b, "window") != NULL && strstr(b, "text UI") != NULL, "window text: %s", b);
    mui_set_probe(probe_yes);
    hooks = (const sel_tag*)sel_tag_find(sel_plugin_entry_mui_test(1u), SEL_PLUGIN_TAGS, 0);
    open = (sel_ui_open_fn)sel_tag_find(hooks, SEL_UI_OPEN, 0);
    CHECK(open != NULL && open(NULL) == NULL, "open does not fail without a window system");
}

/* mui-menu (MUI-P2-3): rows = action rows with a menu path, every key shown, titles non-empty. */
static void test_menu(void) {
    static mui_menu_item items[512];
    uint32_t i, found_sub, want = 0, n = sel_uim_action_count(), got = mui_menu_build(items, 512);
    for (i = 0; i < n; i++) {
        want += sel_uim_action_at(i)->menu[0] != '\0';
    }
    CHECK(want > 20u && got == want, "menu rows %u, action rows with a menu path %u", (unsigned)got, (unsigned)want);
    for (i = 0; i < got; i++) {
        CHECK(items[i].title[0] != '\0' && items[i].item[0] != '\0', "row %u has no title or item", (unsigned)i);
    }
    for (i = 0; i < n && got == want; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        uint32_t k, found = 0;
        if (a->menu[0] == '\0' || a->key[0] == '\0') {
            continue;
        }
        for (k = 0; k < got; k++) {
            found += strcmp(items[k].id, a->id) == 0 && strcmp(items[k].key, a->key) == 0 &&
                     (a->key[1] != '\0' || items[k].shortcut == a->key[0]);
        }
        CHECK(found == 1u, "key %s of %s is not a shortcut", a->key, a->id);
    }
    for (i = 0, found_sub = 0; i < got; i++) {
        found_sub += strcmp(items[i].sub, "Visualizer") == 0 && strchr(items[i].item, '/') == NULL;
    }
    CHECK(found_sub >= 3u, "View/Visualizer/* rows are not split into sub menu and item (%u)", (unsigned)found_sub);
}

/* mui-menu-dispatch (MUI-P2-4): every menu row sends the table line the desktop sends. */
static void test_menu_dispatch(void) {
    static mui_menu_item items[512];
    uint32_t i, got = mui_menu_build(items, 512);
    CHECK(got > 20u, "no menu rows");
    for (i = 0; i < got; i++) {
        const char* line = mui_menu_line(items[i].id);
        uint32_t k, n = sel_uim_action_count();
        const sel_uim_action* a = NULL;
        for (k = 0; k < n; k++) {
            if (strcmp(sel_uim_action_at(k)->id, items[i].id) == 0) {
                a = sel_uim_action_at(k);
            }
        }
        CHECK(a != NULL && line != NULL && line[0] != '\0' && strcmp(line, a->line) == 0, "menu %s/%s sends '%s'", items[i].title, items[i].item, line != NULL ? line : "(null)");
    }
    CHECK(mui_menu_line("no_such_action") == NULL, "unknown id has a line");
}

/* mui-weights (MUI-P2-1): horizontal weights equal the region widths and sum to the window width. */
static void test_weights(void) {
    static const uint32_t widths[] = {1280, 1024, 800, 640};
    uint32_t i;
    for (i = 0; i < 4; i++) {
        mui_layout_map_t m;
        sel_uim_layout l;
        uint32_t sum, s;
        mui_layout_map(&m, widths[i], 720);
        sel_uim_layout_compute(&l, widths[i], 720);
        sum = m.weight_w[MUI_SLOT_SIDEBAR] + m.weight_w[MUI_SLOT_MAIN] + m.weight_w[MUI_SLOT_DETAIL];
        CHECK(sum == widths[i], "width %u: body weights sum %u", widths[i], sum);
        for (s = 0; s < l.count; s++) {
            const char* n = l.regions[s].name;
            uint32_t slot = strcmp(n, "sidebar") == 0 ? MUI_SLOT_SIDEBAR : strcmp(n, "main") == 0 ? MUI_SLOT_MAIN : strcmp(n, "detail") == 0 ? MUI_SLOT_DETAIL : MUI_SLOT_COUNT;
            if (slot != MUI_SLOT_COUNT) {
                CHECK(m.weight_w[slot] == l.regions[s].w, "width %u: %s weight %u != region %u", widths[i], n, m.weight_w[slot], l.regions[s].w);
            }
        }
        CHECK(m.weight_h[MUI_SLOT_PLAYER] + m.weight_h[MUI_SLOT_MAIN] + m.weight_h[MUI_SLOT_SUGGESTION] == 720, "width %u: vertical weights", widths[i]);
        CHECK(m.balance_sidebar_main == (m.show[MUI_SLOT_SIDEBAR] && m.show[MUI_SLOT_MAIN]), "balance sidebar");
        CHECK(m.balance_main_detail == (widths[i] >= 1200), "balance detail at %u", widths[i]);
    }
}

/* mui-relayout (MUI-P2-2): the shown set per breakpoint; the slot count is fixed, and sel_uim_breakpoint_for decides. */
static void test_relayout(void) {
    static const uint32_t widths[] = {1280, 1024, 800, 640};
    static const uint8_t want[4][MUI_SLOT_COUNT] = {{0, 1, 1, 1, 1}, {0, 1, 1, 0, 1}, {0, 1, 1, 0, 1}, {1, 0, 1, 0, 1}};
    mui_layout_map_t m;
    uint32_t i, s, flips = 99;
    mui_layout_map(&m, 1280, 720);
    for (i = 0; i < 4; i++) {
        uint32_t changed = mui_layout_relayout(&m, widths[i], 720, &flips);
        CHECK(m.breakpoint == sel_uim_breakpoint_for(widths[i]), "breakpoint at %u", widths[i]);
        CHECK(changed == (i != 0 && sel_uim_breakpoint_for(widths[i]) != sel_uim_breakpoint_for(widths[i - 1])), "changed flag at %u", widths[i]);
        for (s = 0; s < MUI_SLOT_COUNT; s++) {
            CHECK(m.show[s] == want[i][s], "width %u slot %u show %u", widths[i], s, m.show[s]);
        }
    }
    mui_layout_map(&m, 1280, 720);
    CHECK(mui_layout_relayout(&m, 1100, 720, &flips) == 1 && flips == 1, "1280 to 1100 hides only detail (flips %u)", flips);
    CHECK(mui_layout_relayout(&m, 1000, 720, &flips) == 0 && flips == 0, "same breakpoint changes no visibility");
}

/* mui-transport-map (MUI-P2-5): the buttons reach exactly the action rows whose icon is a transport icon. */
static int is_transport_icon(const sel_uim_action* a) {
    const char* icon = a->icon;
    static const char* const icons[] = {"play", "pause", "square", "skip-forward", "skip-back", "repeat", "shuffle"};
    uint32_t i;
    if (strncmp(a->menu, "Playback/", 9) != 0) { /* the queue shuffle shares the icon but is not transport */
        return 0;
    }
    for (i = 0; i < sizeof icons / sizeof icons[0]; i++) {
        if (strcmp(icon, icons[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_transport(void) {
    uint32_t i, b, play, rep, rnd, n = sel_uim_action_count(), want = 0, got = 0;
    const char* reached[32];
    uint32_t nreached = 0;
    for (i = 0; i < n; i++) {
        want += is_transport_icon(sel_uim_action_at(i)) ? 1u : 0u;
    }
    for (b = 0; b < MUI_BTN_COUNT; b++) {
        for (play = 0; play < 2; play++) {
            for (rep = 0; rep < 3; rep++) {
                for (rnd = 0; rnd < 2; rnd++) {
                    sel_uim_frame f;
                    const char* id;
                    uint32_t k, seen = 0;
                    memset(&f, 0, sizeof f);
                    f.play = play ? SEL_PLAYING : SEL_PAUSED;
                    f.repeat_mode = rep;
                    f.random_on = rnd;
                    id = mui_transport_action(b, &f);
                    CHECK(id != NULL && mui_transport_label(b, &f) != NULL, "button %u has no action or label", b);
                    for (k = 0; id != NULL && k < nreached; k++) {
                        seen |= strcmp(reached[k], id) == 0;
                    }
                    if (id != NULL && !seen && nreached < 32) {
                        reached[nreached++] = id;
                    }
                }
            }
        }
    }
    for (i = 0; i < n; i++) {
        const sel_uim_action* a = sel_uim_action_at(i);
        uint32_t k, hit = 0;
        for (k = 0; k < nreached; k++) {
            hit |= strcmp(reached[k], a->id) == 0;
        }
        got += hit;
        CHECK(hit == (uint32_t)is_transport_icon(a), "action %s: reached %u, transport icon %d", a->id, hit, is_transport_icon(a));
    }
    CHECK(got == want && nreached == want, "button set %u, reached %u, transport rows %u", got, nreached, want);
    CHECK(MUI_SEEK_MAX == 65536 && MUI_VOLUME_MAX == 100, "slider ranges");
    {
        sel_uim_frame f;
        memset(&f, 0, sizeof f);
        f.play = SEL_PLAYING;
        CHECK(strcmp(mui_transport_label(MUI_BTN_PLAY, &f), "Pause") == 0, "playing shows Pause");
        f.play = SEL_STOPPED;
        CHECK(strcmp(mui_transport_label(MUI_BTN_PLAY, &f), "Play") == 0, "stopped shows Play");
    }
    {
        char line[32];
        CHECK(mui_seek_line(32768, 200000, line, sizeof line) == 1 && strcmp(line, "SEEK 100000") == 0, "seek half: %s", line);
        CHECK(mui_seek_line(99999, 1000, line, sizeof line) == 1 && strcmp(line, "SEEK 1000") == 0, "seek clamps: %s", line);
        CHECK(mui_seek_line(100, 0, line, sizeof line) == 0 && line[0] == '\0', "no length, no seek line");
        mui_volume_line(40, line, sizeof line);
        CHECK(strcmp(line, "VOLUME 40") == 0, "volume: %s", line);
        mui_volume_line(250, line, sizeof line);
        CHECK(strcmp(line, "VOLUME 100") == 0, "volume clamps: %s", line);
    }
}

/* mui-dirty-map (MUI-P2-6): ten attributes, each in exactly one region; every region but the queue has one. */
static void test_dirty(void) {
    uint32_t a, covered = 0, bit;
    CHECK(MUI_ATTR_COUNT == 10, "attribute count %u", (unsigned)MUI_ATTR_COUNT);
    for (a = 0; a < MUI_ATTR_COUNT; a++) {
        uint32_t r = mui_attr_region(a), b2 = 0, hits = 0;
        CHECK(r != 0 && (r & (r - 1u)) == 0 && (r & SEL_UIM_DIRTY_ALL) == r, "attr %u region %x", a, (unsigned)r);
        for (bit = 0; bit < SEL_UIM_REGIONS; bit++) {
            b2 = 1u << bit;
            hits += (mui_dirty_attrs(b2) >> a) & 1u;
        }
        CHECK(hits == 1, "attr %u refreshed by %u regions", a, hits);
        covered |= r;
    }
    CHECK(covered == (SEL_UIM_DIRTY_ALL & ~SEL_UIM_DIRTY_QUEUE), "regions covered %x", (unsigned)covered);
    CHECK(mui_dirty_attrs(0) == 0 && mui_dirty_attrs(SEL_UIM_DIRTY_QUEUE) == 0, "queue or clean dirties an attribute");
    {
        sel_uim_frame f;
        memset(&f, 0, sizeof f);
        strcpy(f.title, "T");
        strcpy(f.status, "S");
        CHECK(strcmp(mui_attr_text(MUI_ATTR_TITLE, &f), "T") == 0 && strcmp(mui_attr_text(MUI_ATTR_STATUS, &f), "S") == 0, "attr text");
        CHECK(mui_attr_text(MUI_ATTR_SEEK, &f) == NULL, "slider attr has text");
    }
}

/* mui-keys (MUI-P2-7): every action key is one binding, no two bindings share code and modifiers, mode keys unique. */
static void test_keys(void) {
    static mui_key_bind kb[128];
    uint32_t i, j, n = sel_uim_action_count(), withkey = 0, count, modes;
    const mui_mode_key* mk;
    for (i = 0; i < n; i++) {
        withkey += sel_uim_action_at(i)->key[0] != '\0';
    }
    count = mui_keys_build(kb, 128);
    CHECK(count == withkey && count <= 128, "bindings %u, actions with a key %u", count, withkey);
    for (i = 0; i < count && i < 128; i++) {
        for (j = i + 1; j < count && j < 128; j++) {
            CHECK(kb[i].code != kb[j].code || kb[i].mods != kb[j].mods, "%s and %s share a key", kb[i].id, kb[j].id);
        }
    }
    mk = mui_mode_keys(&modes);
    CHECK(modes == 6, "mode keys %u", modes);
    for (i = 0; i < modes; i++) {
        CHECK(strstr(mk[i].line, mk[i].mode) != NULL, "mode %s line", mk[i].mode);
        for (j = 0; j < count && j < 128; j++) {
            /* a mode key is Right Amiga + char; a single-character action key is the same shortcut when equal */
            CHECK(!kb[j].shortcut || (kb[j].code | 0x20u) != ((uint32_t)mk[i].key | 0x20u), "mode key %c collides with %s", mk[i].key, kb[j].id);
        }
        for (j = i + 1; j < modes; j++) {
            CHECK(mk[i].key != mk[j].key, "duplicate mode key");
        }
    }
}

/* MUI-P3-1..4, P3-5 (mask), P3-7: list models over a fake sel_view_state. */
static void fake_state(sel_view_state* v, sel_view_row* rows, char (*txt)[32], uint32_t total, uint32_t start, uint32_t win, uint32_t style) {
    uint32_t i, n = total - start < win ? total - start : win;
    memset(v, 0, sizeof *v);
    for (i = 0; i < n; i++) {
        memset(&rows[i], 0, sizeof rows[i]);
        snprintf(txt[i], 32, "Song %u", (unsigned)(start + i));
        rows[i].text = txt[i];
        rows[i].artist = "Artist";
        rows[i].id = start + i;
        rows[i].kind = SEL_ROW_QUEUE;
    }
    v->style = style;
    v->row_count = total;
    v->window_start = start;
    v->window_rows = win;
    v->row_n = n;
    v->rows = rows;
    v->cursor = start + 2;
}

static void test_list_regular(void) {
    static sel_view_row rows[40];
    static char txt[40][32];
    static mui_list_entry out[40];
    sel_view_state v;
    int32_t active = -9;
    uint32_t n;
    fake_state(&v, rows, txt, 5000, 1200, 30, SEL_VIEW_STYLE_REGULAR);
    n = mui_list_build(mui_list_backend_pick(1, 1), out, 40, &v, &active);
    CHECK(n == 30 && active == 2, "window %u entries, active %d", n, (int)active);
    CHECK(out[0].index == 1200 && strcmp(out[0].text, "Song 1200") == 0, "first entry %u '%s'", out[0].index, out[0].text);
    n = mui_list_build(mui_list_backend_pick(1, 1), out, 10, &v, &active);
    CHECK(n == 10, "cap limits entries to %u", n);
    v.cursor = 3;
    mui_list_build(mui_list_backend_pick(1, 1), out, 40, &v, &active);
    CHECK(active == -1, "cursor outside window gives %d", (int)active);
}

static void test_list_tree(void) {
    static sel_view_row rows[4];
    static mui_list_entry out[4];
    sel_view_state v;
    char line[32];
    memset(rows, 0, sizeof rows);
    rows[0].kind = SEL_ROW_NODE; rows[0].text = "folder"; rows[0].id = 7; rows[0].count = 3; rows[0].expanded = 1;
    rows[1].kind = SEL_ROW_QUEUE; rows[1].text = "one.mod"; rows[1].depth = 1; rows[1].id = 11;
    rows[2].kind = SEL_ROW_NODE; rows[2].text = "other"; rows[2].id = 8; rows[2].count = 2;
    memset(&v, 0, sizeof v);
    v.style = SEL_VIEW_STYLE_TREE; v.row_count = 3; v.window_rows = 4; v.row_n = 3; v.rows = rows;
    mui_list_build(mui_list_backend_pick(0, 0), out, 4, &v, NULL);
    CHECK(strcmp(out[0].text, "- folder (3)") == 0, "expanded node '%s'", out[0].text);
    CHECK(strcmp(out[1].text, "  one.mod") == 0, "indented song '%s'", out[1].text);
    CHECK(strcmp(out[2].text, "+ other (2)") == 0, "collapsed node '%s'", out[2].text);
    mui_list_toggle_line(&rows[0], line, sizeof line);
    CHECK(strcmp(line, "VIEW_COLLAPSE 7") == 0, "collapse line '%s'", line);
    mui_list_toggle_line(&rows[2], line, sizeof line);
    CHECK(strcmp(line, "VIEW_EXPAND 8") == 0, "expand line '%s'", line);
    mui_list_toggle_line(&rows[1], line, sizeof line);
    CHECK(line[0] == '\0', "song row toggles nothing");
    mui_list_build(mui_list_backend_pick(1, 1), out, 4, &v, NULL);
    CHECK(strcmp(out[1].text, "one.mod") == 0, "Listtree draws depth itself: '%s'", out[1].text);
}

static void test_list_drill(void) {
    sel_view_state v;
    char buf[160];
    memset(&v, 0, sizeof v);
    v.source = SEL_VIEW_QUEUE; v.style = SEL_VIEW_STYLE_DRILLDOWN; v.group = "folder"; v.drill_n = 2;
    v.drill[0] = "a"; v.drill[1] = "b";
    mui_list_breadcrumb(&v, buf, sizeof buf);
    CHECK(strcmp(buf, "Queue by folder  Drilldown > a > b") == 0, "breadcrumb '%s'", buf);
}

static void test_columns(void) {
    uint8_t c[MUI_COL_COUNT];
    char f[32];
    static const uint32_t want[4] = {5, 4, 3, 2};
    uint32_t bp;
    for (bp = 0; bp < 4; bp++) {
        CHECK(mui_columns_for(bp, c) == want[bp] && c[0] == MUI_COL_TITLE, "breakpoint %u has %u columns", bp, mui_columns_for(bp, c));
        mui_columns_format(bp, f, sizeof f);
        CHECK(strlen(f) == 4u * (want[bp] - 1u), "format '%s' at %u", f, bp);
    }
}

static void test_style_mask(void) {
    CHECK(__builtin_popcount(mui_style_mask(1)) == 6 && __builtin_popcount(mui_style_mask(0)) == 5, "mask sizes %d %d", __builtin_popcount(mui_style_mask(1)), __builtin_popcount(mui_style_mask(0)));
    CHECK((mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COVERFLOW)) == 0 && (mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_GRID)) != 0, "planar drops only cover flow");
}

static void test_list_backends(void) {
    static sel_view_row rows[40];
    static char txt[40][32];
    static mui_list_entry a[40], b[40];
    sel_view_state v;
    int32_t ia, ib;
    uint32_t i, n, m, ca, cb;
    uint8_t col[MUI_COL_COUNT];
    fake_state(&v, rows, txt, 100, 10, 20, SEL_VIEW_STYLE_REGULAR);
    n = mui_list_build(mui_list_backend_pick(1, 1), a, 40, &v, &ia);
    m = mui_list_build(mui_list_backend_pick(0, 0), b, 40, &v, &ib);
    CHECK(n == m && ia == ib, "row count %u/%u cursor %d/%d", n, m, (int)ia, (int)ib);
    for (i = 0; i < n; i++) {
        CHECK(strcmp(a[i].text, b[i].text) == 0, "row %u differs", i);
    }
    for (i = 0; i < 4; i++) {
        ca = mui_columns_for(i, col);
        cb = mui_columns_for(i, col);
        CHECK(ca == cb, "column set differs");
    }
    CHECK(mui_list_backend_pick(1, 1)->sortable == 1 && mui_list_backend_pick(0, 0)->sortable == 0, "only NList sorts");
}

static void test_home(void) {
    sel_view_row r[4];
    sel_view_state v;
    char lines[3][48];
    memset(r, 0, sizeof r);
    memset(&v, 0, sizeof v);
    r[0].kind = SEL_ROW_HOME; r[0].key = "a/b.mod";
    r[1].kind = SEL_ROW_HOME; r[1].key = "Album"; r[1].album = 1;
    r[2].kind = SEL_ROW_HOME; r[2].key = "canon"; r[2].composer = 1;
    r[3].kind = SEL_ROW_HEADING;
    v.rows = r; v.row_n = 4; v.window_start = 10;
    CHECK(mui_home_kind_of(&r[0]) == MUI_HOME_SONG && mui_home_kind_of(&r[1]) == MUI_HOME_ALBUM, "song versus album");
    CHECK(mui_home_kind_of(&r[2]) == MUI_HOME_COMPOSER && mui_home_kind_of(&r[3]) == MUI_HOME_NONE, "composer, heading");
    CHECK(mui_home_lines(&v, 1, lines) == 3 && strcmp(lines[0], "VIEW_CURSOR 11") == 0 && strcmp(lines[1], "VIEW_ACTIVATE") == 0 &&
              strcmp(lines[2], "VIEW_SOURCE home") == 0, "lines '%s' '%s'", lines[0], lines[1]);
    CHECK(mui_home_lines(&v, 3, lines) == 0 && mui_home_lines(&v, 9, lines) == 0, "heading or outside window sends nothing");
}

static void test_library(void) {
    mui_crumb c[8];
    CHECK(mui_path_crumbs("/home/x/songs", c, 8) == 3 && strcmp(c[0].name, "home") == 0 && strcmp(c[2].name, "songs") == 0 && c[2].end == 13u,
          "unix crumbs");
    CHECK(mui_path_crumbs("Work:Music/Mods", c, 8) == 3 && strcmp(c[0].name, "Work:") == 0 && c[0].end == 5u && strcmp(c[1].name, "Music") == 0 &&
              strcmp(c[2].name, "Mods") == 0 && c[2].end == 15u, "AmigaDOS crumbs %s %s", c[0].name, c[1].name);
    CHECK(mui_path_crumbs("Work:", c, 8) == 1 && strcmp(c[0].name, "Work:") == 0, "volume root");
    CHECK(mui_path_crumbs("/", c, 8) == 0 && mui_path_crumbs("", c, 8) == 0 && mui_path_crumbs(NULL, c, 8) == 0, "empty roots");
    CHECK(mui_path_crumbs("a/b/c", c, 2) == 3, "count above cap");
}

static void test_toast(void) {
    mui_toast t;
    memset(&t, 0, sizeof t);
    CHECK(mui_toast_ms(0) == 3000u && mui_toast_ms(1) == 6000u, "durations");
    mui_toast_show(&t, "first", 0, 1000);
    mui_toast_show(&t, "second", 1, 2000);
    CHECK(t.visible && strcmp(t.text, "second") == 0 && t.until_ms == 8000u && t.shown == 2u, "replace, not stack: '%s' %u", t.text, t.until_ms);
    CHECK(mui_toast_tick(&t, 7999) == 0 && t.visible, "still visible");
    CHECK(mui_toast_tick(&t, 8000) == 1 && !t.visible && t.text[0] == '\0', "gone at 6 s");
}

static void test_help(void) {
    char row[128], msg[128];
    uint32_t i;
    CHECK(mui_help_count() == sel_uim_action_count() && mui_help_count() > 20u, "help rows %u", (unsigned)mui_help_count());
    CHECK(sel_uim_actions_check(msg, sizeof msg) == 0, "action table check: %s", msg);
    for (i = 0; i < mui_help_count(); i++) {
        mui_help_row(i, row, sizeof row);
        CHECK(row[0] != '\0', "help row %u empty", (unsigned)i);
    }
}

static void test_search(void) {
    char l[3][160];
    CHECK(mui_search_lines("abc", l) == 3 && strcmp(l[0], "LIBRARY_SEARCH \"abc\"") == 0 && strcmp(l[1], "VIEW_SOURCE search") == 0, "search '%s'", l[0]);
    CHECK(mui_search_lines("a\"b\\", l) == 3 && strcmp(l[0], "LIBRARY_SEARCH \"a\\\"b\\\\\"") == 0, "escape '%s'", l[0]);
}

static void test_queue(void) {
    char o[64];
    mui_queue_line("move", 7, 9, o, sizeof o);
    CHECK(strcmp(o, "QUEUE_MOVE 7 9") == 0, "move '%s'", o);
    mui_queue_line("remove", 7, 0, o, sizeof o);
    CHECK(strcmp(o, "QUEUE_REMOVE 7") == 0, "remove '%s'", o);
    mui_queue_line("clear", 0, 0, o, sizeof o);
    CHECK(strcmp(o, "QUEUE_CLEAR") == 0, "clear");
    mui_queue_line("bogus", 0, 0, o, sizeof o);
    CHECK(o[0] == '\0', "unknown verb sends nothing");
}

static void test_playlists(void) {
    char o[64];
    CHECK(mui_playlist_line("open", 3, NULL, o, sizeof o) == 1 && strcmp(o, "VIEW_ACTIVATE") == 0, "open");
    CHECK(mui_playlist_line("rename", 3, "Mine", o, sizeof o) == 1 && strcmp(o, "PLAYLIST_RENAME 3 \"Mine\"") == 0, "rename '%s'", o);
    CHECK(mui_playlist_line("delete", 3, NULL, o, sizeof o) == 1 && strcmp(o, "PLAYLIST_DEL 3") == 0, "delete '%s'", o);
    CHECK(mui_playlist_line("zap", 3, NULL, o, sizeof o) == 0, "unknown action");
}

static void test_song_menu(void) {
    /* The desktop's song menu labels come from the same function; the set is the five items, item 4 switches on the Queue page. */
    static const char* const want[5] = {"Play", "Play Next", "Add to Queue", "Add to Playlist", "Remove from Library"};
    uint32_t i;
    for (i = 0; i < SEL_UIM_SONG_MENU_ITEMS; i++) {
        CHECK(strcmp(sel_uim_song_menu_label(i, 0), want[i]) == 0, "item %u '%s'", (unsigned)i, sel_uim_song_menu_label(i, 0));
    }
    CHECK(strcmp(sel_uim_song_menu_label(4, 1), "Remove from Queue") == 0 && sel_uim_song_menu_label(5, 0) == NULL, "queue page, bound");
}

static void test_like_rating(void) {
    char o[32];
    uint32_t i;
    CHECK(strcmp(mui_like_line(), "LIKE") == 0, "like");
    for (i = 0; i <= 5; i++) {
        mui_rating_line(i, o, sizeof o);
        CHECK(strcmp(o, i == 0 ? "RATE 0" : i == 1 ? "RATE 1" : i == 2 ? "RATE 2" : i == 3 ? "RATE 3" : i == 4 ? "RATE 4" : "RATE 5") == 0, "rate %u '%s'", (unsigned)i, o);
    }
    mui_rating_line(9, o, sizeof o);
    CHECK(strcmp(o, "RATE 5") == 0, "clamped");
    sel_uim_rate_line(3, 3, o, sizeof o);
    CHECK(strcmp(o, "RATE 0") == 0, "same star clears");
    sel_uim_rate_line(3, 4, o, sizeof o);
    CHECK(strcmp(o, "RATE 4") == 0, "other star sets");
}

static void test_nowplaying(void) {
    const char* names[8];
    CHECK(mui_np_objects(names, 8) == 6 && strcmp(names[0], "artwork") == 0 && strcmp(names[4], "lyrics") == 0 && strcmp(names[5], "upnext") == 0, "object set");
    CHECK(mui_lyric_line_of(0) == -1 && mui_lyric_line_of(1) == 0 && mui_lyric_line_of(12) == 11 && mui_lyric_line_of(-3) == -1, "slot to line");
}

static void test_songinfo(void) {
    char o[32];
    CHECK(mui_subsong_cycle_line(1, 3, o, sizeof o) == 1 && strcmp(o, "SUBSONG 2") == 0, "next '%s'", o);
    CHECK(mui_subsong_cycle_line(3, 3, o, sizeof o) == 1 && strcmp(o, "SUBSONG 1") == 0, "wraps '%s'", o);
    CHECK(mui_subsong_cycle_line(1, 1, o, sizeof o) == 0, "single subsong has no cycle");
}

static void test_settings(void) {
    uint32_t i, n = sel_uim_pref_row_count();
    char o[96];
    CHECK(n >= 6u, "rows %u", (unsigned)n);
    for (i = 0; i < n; i++) {
        const sel_uim_pref_row* r = sel_uim_pref_row_at(i);
        int found = 0;
#define SEL_PREF(id, pkey, def, kind) if (strcmp(pkey, r->key) == 0) found = 1;
#include <selecta/prefs.def>
#undef SEL_PREF
        CHECK(found, "pref %s is not in prefs.def", r->key);
        CHECK(r->ui <= SEL_UIM_PREF_STRING && r->label[0] != '\0', "ui type and label of %s", r->key);
        CHECK(r->ui == SEL_UIM_PREF_STRING || r->choices[0] != '\0', "choices of %s", r->key);
    }
    sel_uim_pref_line("mix.loudness", "off", o, sizeof o);
    CHECK(strcmp(o, "PREF_SET mix.loudness \"off\"") == 0, "line '%s'", o);
}

static void test_dialogs(void) {
    char o[200];
    mui_confirm_text(1, NULL, 12, 3, o, sizeof o);
    CHECK(strstr(o, "Remove 12 songs and 3 playlists, with all ratings and notes?") != NULL, "clear text '%s'", o);
    mui_confirm_text(0, "Song A", 0, 0, o, sizeof o);
    CHECK(strstr(o, "Remove \"Song A\" from the library?") != NULL, "remove text '%s'", o);
    CHECK(strcmp(mui_confirm_line(1), "LIBRARY_CLEAR") == 0 && mui_confirm_line(0) == NULL, "clear sends LIBRARY_CLEAR");
}

static int g_sent;
static char g_sent_line[200];
static int fake_send(void* ctx, const char* line) {
    (void)ctx;
    g_sent++;
    snprintf(g_sent_line, sizeof g_sent_line, "%s", line);
    return 1;
}

static void test_cmdline(void) {
    g_sent = 0;
    CHECK(mui_cmdline_submit(fake_send, NULL, "PLAY  \"a b\"  ") == 1 && g_sent == 1 && strcmp(g_sent_line, "PLAY  \"a b\"  ") == 0, "unchanged '%s'", g_sent_line);
    CHECK(mui_cmdline_submit(fake_send, NULL, "") == 0 && g_sent == 1, "empty line is not sent");
}

static void test_states(void) {
    uint32_t v, k;
    for (v = 0; v < SEL_UIM_SV_COUNT; v++) {
        for (k = 0; k < 3; k++) {
            const char* t = sel_uim_state_text(v, k);
            CHECK(t != NULL && t[0] != '\0', "state %u/%u", (unsigned)v, (unsigned)k);
        }
    }
    CHECK(strcmp(sel_uim_state_text(SEL_UIM_SV_QUEUE, SEL_UIM_ST_EMPTY), "The queue is empty. Add songs from the Library.") == 0, "queue empty");
    CHECK(sel_uim_state_text(99, 0) == NULL && sel_uim_state_text(0, 9) == NULL, "unknown pair");
}

static void test_tracker(void) {
    sel_rp_cell c;
    char o[40];
    memset(&c, 0, sizeof c);
    c.note = 49; c.instrument = 1; c.effect = 0xA; c.param = 0x0F;
    mui_tracker_cell_text(&c, o, sizeof o);
    CHECK(strstr(o, " 01 .. A0F") != NULL, "cell '%s'", o);
    memset(&c, 0, sizeof c);
    mui_tracker_cell_text(&c, o, sizeof o);
    CHECK(strstr(o, " .. .. ...") != NULL, "empty cell '%s'", o);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    if (strcmp(argv[1], "wait") == 0) {
        test_wait();
    } else if (strcmp(argv[1], "registry") == 0) {
        test_registry();
    } else if (strcmp(argv[1], "failure") == 0) {
        test_failure();
    } else if (strcmp(argv[1], "menu") == 0) {
        test_menu();
    } else if (strcmp(argv[1], "menu-dispatch") == 0) {
        test_menu_dispatch();
    } else if (strcmp(argv[1], "weights") == 0) {
        test_weights();
    } else if (strcmp(argv[1], "relayout") == 0) {
        test_relayout();
    } else if (strcmp(argv[1], "transport") == 0) {
        test_transport();
    } else if (strcmp(argv[1], "dirty") == 0) {
        test_dirty();
    } else if (strcmp(argv[1], "keys") == 0) {
        test_keys();
    } else if (strcmp(argv[1], "list-regular") == 0) {
        test_list_regular();
    } else if (strcmp(argv[1], "list-tree") == 0) {
        test_list_tree();
    } else if (strcmp(argv[1], "list-drill") == 0) {
        test_list_drill();
    } else if (strcmp(argv[1], "columns") == 0) {
        test_columns();
    } else if (strcmp(argv[1], "style-mask") == 0) {
        test_style_mask();
    } else if (strcmp(argv[1], "list-backends") == 0) {
        test_list_backends();
    } else if (strcmp(argv[1], "home") == 0) {
        test_home();
    } else if (strcmp(argv[1], "library") == 0) {
        test_library();
    } else if (strcmp(argv[1], "toast") == 0) {
        test_toast();
    } else if (strcmp(argv[1], "help") == 0) {
        test_help();
    } else if (strcmp(argv[1], "search") == 0) {
        test_search();
    } else if (strcmp(argv[1], "queue") == 0) {
        test_queue();
    } else if (strcmp(argv[1], "playlists") == 0) {
        test_playlists();
    } else if (strcmp(argv[1], "song-menu") == 0) {
        test_song_menu();
    } else if (strcmp(argv[1], "like-rating") == 0) {
        test_like_rating();
    } else if (strcmp(argv[1], "nowplaying") == 0) {
        test_nowplaying();
    } else if (strcmp(argv[1], "songinfo") == 0) {
        test_songinfo();
    } else if (strcmp(argv[1], "settings") == 0) {
        test_settings();
    } else if (strcmp(argv[1], "dialogs") == 0) {
        test_dialogs();
    } else if (strcmp(argv[1], "cmdline") == 0) {
        test_cmdline();
    } else if (strcmp(argv[1], "states") == 0) {
        test_states();
    } else if (strcmp(argv[1], "tracker") == 0) {
        test_tracker();
    } else {
        return 2;
    }
    if (failures == 0) {
        puts("mui_contract_test: ok");
    }
    return failures != 0;
}
