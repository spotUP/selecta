/* mui-wait-contract (MUI-P1-7), mui-registry (MUI-P1-6), mui-open-failure (MUI-P1-4), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_contract_test wait|registry|failure */
#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#include "mui_icons.h"
#include "mui_map.h"
#include "mui_theme.h"

#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/theme.h>

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

/* mui-style-menu (MUI-P3-8): View > Style entries equal the mask popcount (planar 5, RTG 6) and each entry sends
 * "VIEW_STYLE <label>" for a style in the mask. */

/* I648: the MUI look comes from the desktop UI tokens. The native build path (mui_ui.c) is compiled only on the Amiga, so
 * this test checks both halves it is made of: the token mapping (with a call counter) and that mui_ui.c asks for each role
 * through TH_TAGS and keeps no hardcoded button frame or background. */
static void test_theme(void) {
    txt_theme th;
    mui_theme_look lk;
    char want[40];
    uint32_t before = mui_theme_applied();
    FILE* f;
    char src[400000];
    size_t n;
    int role;
    static const char* const names[MUI_TH_COUNT] = {"MUI_TH_WINDOW", "MUI_TH_PANEL", "MUI_TH_BUTTON", "MUI_TH_FIELD", "MUI_TH_LIST", "MUI_TH_STATUS"};
    txt_theme_default(&th);
    CHECK(mui_theme_look_for(MUI_TH_WINDOW, th.token, &lk) == 1, "window look");
    snprintf(want, sizeof want, "2:%08x,%08x,%08x", (unsigned)(((th.token[SEL_GUI_TOKEN_BACKGROUND] >> 16) & 0xFF) * 0x01010101u),
             (unsigned)(((th.token[SEL_GUI_TOKEN_BACKGROUND] >> 8) & 0xFF) * 0x01010101u), (unsigned)((th.token[SEL_GUI_TOKEN_BACKGROUND] & 0xFF) * 0x01010101u));
    CHECK(strcmp(lk.background, want) == 0, "window background %s want %s", lk.background, want);
    CHECK(lk.frame == MUI_FRAME_NONE, "window frame %u", (unsigned)lk.frame);
    CHECK(mui_theme_look_for(MUI_TH_BUTTON, th.token, &lk) == 1 && lk.bg_rgb == th.token[SEL_GUI_TOKEN_SURFACE_RAISED] && lk.frame == MUI_FRAME_TEXT,
          "button look from SURFACE_RAISED, thin frame");
    CHECK(mui_theme_look_for(MUI_TH_FIELD, th.token, &lk) == 1 && lk.frame == MUI_FRAME_STRING, "field frame");
    CHECK(mui_theme_look_for(MUI_TH_LIST, th.token, &lk) == 1 && lk.bg_rgb == th.token[SEL_GUI_TOKEN_SURFACE], "list look from SURFACE");
    CHECK(mui_theme_look_for(MUI_TH_PANEL, th.token, &lk) == 1 && mui_theme_look_for(MUI_TH_STATUS, th.token, &lk) == 1 && lk.bg_rgb == th.token[SEL_GUI_TOKEN_PANEL],
          "panel and status looks");
    CHECK(mui_theme_applied() - before == 6, "applied count %u", (unsigned)(mui_theme_applied() - before));
    CHECK(mui_theme_look_for(MUI_TH_COUNT, th.token, &lk) == 0 && mui_theme_look_for(MUI_TH_WINDOW, NULL, &lk) == 0, "bad input refused");
    CHECK(mui_theme_readable(th.token[SEL_GUI_TOKEN_TEXT], th.token[SEL_GUI_TOKEN_BACKGROUND]) == 1, "light text on the dark theme is readable");
    CHECK(mui_theme_readable(0x000000u, th.token[SEL_GUI_TOKEN_BACKGROUND]) == 0, "black MUI text on the dark theme is not readable: theme stays off");
    f = fopen(MUI_UI_SOURCE, "rb");
    CHECK(f != NULL, "open %s", MUI_UI_SOURCE);
    if (f == NULL) {
        return;
    }
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    for (role = 0; role < MUI_TH_COUNT; role++) {
        char key[64];
        snprintf(key, sizeof key, "TH_TAGS(%s,", names[role]);
        CHECK(strstr(src, key) != NULL, "mui_ui.c never builds role %s through TH_TAGS", names[role]);
    }
    CHECK(strstr(src, "MUIA_Frame, MUIV_Frame_Button") == NULL, "button frame is hardcoded in mui_ui.c");
    CHECK(strstr(src, "MUIA_Background, MUII_ButtonBack") == NULL, "button background is hardcoded in mui_ui.c");
}

static void test_style_menu(void) {
    static mui_menu_item items[512];
    uint32_t pass, i, n;
    for (pass = 0; pass < 2; pass++) {
        uint32_t mask = mui_style_mask((int)pass), want = 0, got = 0, s;
        for (s = 0; s < SEL_VIEW_STYLE_COUNT; s++) {
            want += (mask & SEL_VIEW_STYLE_BIT(s)) != 0u;
        }
        n = mui_menu_build_for(items, 512, mask);
        for (i = 0; i < n && i < 512u; i++) {
            if (strcmp(items[i].title, "View") == 0 && strcmp(items[i].sub, "Style") == 0) {
                const char* line = mui_menu_line(items[i].id);
                uint32_t k, hit = 0;
                for (k = 0; k < SEL_VIEW_STYLE_COUNT; k++) {
                    char want_line[48];
                    snprintf(want_line, sizeof want_line, "VIEW_STYLE %s", sel_view_style_label(k));
                    hit += line != NULL && strcmp(line, want_line) == 0 && (mask & SEL_VIEW_STYLE_BIT(k)) != 0u;
                }
                CHECK(hit == 1u, "style entry %s sends %s, not a style of the mask", items[i].item, line != NULL ? line : "(null)");
                got++;
            }
        }
        CHECK(got == want, "mask %u: %u style entries, %u styles offered", (unsigned)pass, (unsigned)got, (unsigned)want);
    }
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
    char f[48];
    static const uint32_t want[4] = {5, 4, 3, 2};
    uint32_t bp;
    for (bp = 0; bp < 4; bp++) {
        CHECK(mui_columns_for(bp, c) == want[bp] && c[0] == MUI_COL_TITLE, "breakpoint %u has %u columns", bp, mui_columns_for(bp, c));
        mui_columns_format(bp, f, sizeof f);
        {
            uint32_t bars = 0, k;
            for (k = 0; f[k] != '\0'; k++) {
                bars += strncmp(f + k, "BAR,", 4) == 0;
            }
            CHECK(bars == want[bp] - 1u, "format '%s' has %u BAR separators at %u", f, bars, bp);
            CHECK(strncmp(f, "W=40 BAR,W=25", 13) == 0, "title and artist carry weights: '%s' at %u", f, bp);
        }
    }
}

/* MUI-P3-4: the album, length and format cells come from the snapshot, not from the (title, artist) engine row. */
static void test_row_cells(void) {
    static sel_state st;
    static const char arena[] = "RAM:mui-mm/meme.mod\0Work:other/x.xm";
    sel_queue_entry q[2];
    sel_view_row r;
    mui_row_extra x;
    memset(&st, 0, sizeof st);
    memset(&r, 0, sizeof r);
    q[0].id = 5; q[0].path_offset = 0;
    q[1].id = 6; q[1].path_offset = 20;
    st.queue = q; st.queue_count = 2; st.queue_text = arena;
    st.song.queue_id = 5; st.song.duration_ms = 185000;
    r.kind = SEL_ROW_QUEUE; r.id = 5; r.text = "That Meme Again";
    mui_row_extra_fill(&st, &r, "Evoke", &x);
    CHECK(strcmp(x.format, "mod") == 0 && strcmp(x.length, "03:05") == 0 && strcmp(x.album, "Evoke") == 0, "playing row: '%s' '%s' '%s'", x.format, x.length, x.album);
    r.id = 6;
    mui_row_extra_fill(&st, &r, NULL, &x);
    CHECK(strcmp(x.format, "xm") == 0 && strcmp(x.length, "--:--") == 0 && x.album[0] == '\0', "queued row: '%s' '%s' '%s'", x.format, x.length, x.album);
    r.kind = SEL_ROW_FILE; r.text = "dir.x/readme";
    mui_row_extra_fill(&st, &r, NULL, &x);
    CHECK(x.format[0] == '\0', "no extension after the last slash: '%s'", x.format);
}

static void test_style_mask(void) {
    CHECK(__builtin_popcount(mui_style_mask(1)) == 6 && __builtin_popcount(mui_style_mask(0)) == 5, "mask sizes %d %d", __builtin_popcount(mui_style_mask(1)), __builtin_popcount(mui_style_mask(0)));
    CHECK((mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COVERFLOW)) == 0 && (mui_style_mask(0) & SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_GRID)) != 0, "planar drops only cover flow");
}

/* MUI-P3-1: a rebuild from a snapshot that has not seen the user's VIEW_CURSOR yet keeps the clicked row selected. */
static void test_list_pending(void) {
    mui_list_pending p;
    memset(&p, 0, sizeof p);
    CHECK(mui_list_pending_resolve(&p, 0) == 0, "nothing pending: snapshot wins");
    mui_list_pending_arm(&p, 3, 0); /* user clicked row 3, snapshot cursor still row 0 */
    CHECK(mui_list_pending_resolve(&p, 0) == 3, "stale snapshot must not move the selection back to row 0");
    CHECK(mui_list_pending_resolve(&p, 0) == 3, "still stale on a second rebuild");
    CHECK(mui_list_pending_resolve(&p, 3) == 3 && !p.armed, "snapshot caught up: settled");
    CHECK(mui_list_pending_resolve(&p, 0) == 0, "settled: later snapshot wins");
    mui_list_pending_arm(&p, 2, 0);
    CHECK(mui_list_pending_resolve(&p, 1) == 1 && !p.armed, "snapshot moved elsewhere: snapshot wins");
    mui_list_pending_arm(&p, 2, 0);
    mui_list_pending_clear(&p);
    CHECK(mui_list_pending_resolve(&p, 0) == 0, "cleared (double-click sent): snapshot wins");
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

/* MUI-P4-0: seven doors from the shared table, each with its label; the five main-view pages carry a line the engine accepts. */
static void test_doors(void) {
    static const char* const want[] = {"Home", "Search", "Your Library", "Playlists", "Queue", "Now Playing", "Song Information"};
    uint32_t i, lines = 0;
    CHECK(mui_door_count() == 7u, "seven doors");
    for (i = 0; i < 7u; i++) {
        CHECK(mui_door_label(i) != NULL && strcmp(mui_door_label(i), want[i]) == 0, "door %u label", (unsigned)i);
        lines += mui_door_line(i) != NULL;
    }
    CHECK(lines == 5u && strcmp(mui_door_line(1), "VIEW_SOURCE search") == 0 && mui_door_line(5) == NULL, "five main-view lines");
    CHECK(mui_door_label(7) == NULL && mui_door_line(7) == NULL, "past the table");
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

static void test_view_empty(void) {
    sel_view_row r[2];
    sel_view_state v;
    memset(r, 0, sizeof r);
    memset(&v, 0, sizeof v);
    CHECK(mui_view_is_empty(&v) == 1, "no rows is empty");
    v.rows = r; v.row_n = 1; v.row_count = 1; v.source = SEL_VIEW_SEARCH;
    r[0].kind = SEL_ROW_HEADING; /* "Songs (0)" */
    CHECK(mui_view_is_empty(&v) == 1, "search with only a heading is empty");
    v.source = SEL_VIEW_HOME;
    CHECK(mui_view_is_empty(&v) == 1, "home with only headings is empty");
    r[0].kind = SEL_ROW_HEADING; r[1].kind = SEL_ROW_HOME; v.row_n = 2; v.row_count = 2;
    CHECK(mui_view_is_empty(&v) == 0, "home with an entry is not empty");
    r[0].kind = SEL_ROW_FILE; v.row_n = 1; v.row_count = 1; v.source = SEL_VIEW_BROWSE;
    CHECK(mui_view_is_empty(&v) == 0, "a browse row is not empty");
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
    {
        char o[96];
        CHECK(mui_song_menu_line(0, "A:b\"c.mod", 0, 0, 0, o, sizeof o) > 0 && strcmp(o, "OPEN \"A:b\\\"c.mod\"") == 0, "play '%s'", o);
        CHECK(strcmp(mui_song_menu_follow(0), "PLAY") == 0 && mui_song_menu_follow(2) == NULL, "play follows with PLAY");
        mui_song_menu_line(1, "x.mod", 0, 0, 0, o, sizeof o);
        CHECK(strcmp(o, "QUEUE_PLAY_NEXT \"x.mod\"") == 0, "next '%s'", o);
        mui_song_menu_line(2, "x.mod", 0, 0, 0, o, sizeof o);
        CHECK(strcmp(o, "QUEUE_ADD \"x.mod\"") == 0, "add '%s'", o);
        CHECK(mui_song_menu_line(3, "x.mod", 0, 0, 0, o, sizeof o) == 0, "playlist needs an id");
        mui_song_menu_line(3, "x.mod", 7, 7, 0, o, sizeof o);
        CHECK(strcmp(o, "PLAYLIST_ADD_SONG 7 \"x.mod\"") == 0, "playlist '%s'", o);
        mui_song_menu_line(4, "x.mod", 9, 0, 1, o, sizeof o);
        CHECK(strcmp(o, "QUEUE_REMOVE 9") == 0, "queue remove '%s'", o);
        mui_song_menu_line(4, "x.mod", 0, 0, 0, o, sizeof o);
        CHECK(strcmp(o, "LIBRARY_REMOVE \"x.mod\"") == 0, "library remove '%s'", o);
    }
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
    {
        const sel_uim_pref_row* ov = NULL;
        for (i = 0; i < n; i++)
            if (strcmp(sel_uim_pref_row_at(i)->key, "ui.desktop.overscan") == 0) ov = sel_uim_pref_row_at(i);
        CHECK(ov != NULL && strcmp(ov->label, "Fill Screen (Overscan)") == 0 && ov->ui == SEL_UIM_PREF_CHECK && strcmp(ov->choices, "off,on") == 0, "overscan row");
        if (ov != NULL) {
            sel_uim_pref_line(ov->key, "1", o, sizeof o);
            CHECK(strcmp(o, "PREF_SET ui.desktop.overscan \"1\"") == 0, "overscan toggle line '%s'", o);
        }
    }
    sel_uim_pref_line("mix.loudness", "off", o, sizeof o);
    CHECK(strcmp(o, "PREF_SET mix.loudness \"off\"") == 0, "line '%s'", o);
}

/* mui-drop (MUI-P5-6): the line a dropped file or drawer sends, the clipboard text and its IFF FTXT image. */
static void test_drop(void) {
    char o[64];
    uint8_t iff[64];
    uint32_t n;
    CHECK(mui_drop_line("Work:Music/a b.mod", 0, o, sizeof o) > 0 && strcmp(o, "QUEUE_ADD \"Work:Music/a b.mod\"") == 0, "file line '%s'", o);
    CHECK(mui_drop_line("Work:Music", 1, o, sizeof o) > 0 && strcmp(o, "LIBRARY_SCAN \"Work:Music\"") == 0, "drawer line '%s'", o);
    CHECK(mui_drop_line("a\"b\\c", 0, o, sizeof o) > 0 && strcmp(o, "QUEUE_ADD \"a\\\"b\\\\c\"") == 0, "escape '%s'", o);
    CHECK(mui_drop_line("", 0, o, sizeof o) == 0, "empty path");
    CHECK(mui_drop_line("0123456789012345678901234567890123456789", 0, o, 30) == 0, "too long");
    CHECK(mui_clip_text("Virgill", "That Meme", o, sizeof o) > 0 && strcmp(o, "Virgill - That Meme") == 0, "text '%s'", o);
    CHECK(mui_clip_text("", "Solo", o, sizeof o) > 0 && strcmp(o, "Solo") == 0, "title only '%s'", o);
    CHECK(mui_clip_text("A", "", o, sizeof o) == 0, "no title");
    n = mui_clip_iff("abc", 3, iff, sizeof iff);
    CHECK(n == 24 && memcmp(iff, "FORM", 4) == 0 && iff[7] == 16 && memcmp(iff + 8, "FTXT", 4) == 0 && memcmp(iff + 12, "CHRS", 4) == 0
              && iff[19] == 3 && memcmp(iff + 20, "abc", 3) == 0 && iff[23] == 0, "iff odd length n=%u", (unsigned)n);
    n = mui_clip_iff("abcd", 4, iff, sizeof iff);
    CHECK(n == 24 && iff[7] == 16, "iff even length n=%u", (unsigned)n);
    CHECK(mui_clip_iff("abcd", 4, iff, 23) == 0, "iff too small");
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

/* Regression (MUI-P4-10): eleven buttons and two cycles in one row made the window wider than the 800 pixel screen and
 * MUIA_Window_Open stayed 0 on the Amiga. Each tool row must stay narrow enough. */
static void test_toolrows(void) {
    uint32_t r;
    for (r = 0; r < 2u; r++) {
        if (mui_tool_row_chars(r) == 0u || mui_tool_row_chars(r) > 64u) {
            fprintf(stderr, "FAIL tool row %u holds %u label characters (1..64 fit an 800 pixel window)\n", (unsigned)r, (unsigned)mui_tool_row_chars(r));
            failures++;
        }
    }
}

/* Regression (I656): with the I651 icons every button grew by 24 + 6 + 20 px, the tool row 2 reached ~860 px and MUI refused to
 * open the window (MUIA_Window_Open read back 0 on rig 3). Each button row's summed minimum width, from the constants and the
 * label tables mui_ui.c builds from, must fit the smallest supported screen (640 px, plan P6-2). The rows must also be built
 * from the icon-only call, or the model above would not describe them. */
static void test_icon_button_rows(void) {
    uint32_t r;
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    for (r = 0; r < 2u; r++) {
        CHECK(mui_tool_row_width(r) <= MUI_MIN_SCREEN_W, "tool row %u needs %u px (screen %d)", (unsigned)r, (unsigned)mui_tool_row_width(r), MUI_MIN_SCREEN_W);
    }
    for (r = 0; r < 2u; r++) {
        CHECK(mui_transport_row_width(r) <= MUI_MIN_SCREEN_W, "transport row %u needs %u px", (unsigned)r, (unsigned)mui_transport_row_width(r));
    }
    CHECK(mui_extra_row_width() <= MUI_MIN_SCREEN_W, "extras row needs %u px", (unsigned)mui_extra_row_width());
    /* I656b: icon-only buttons must stretch (spacers around the fixed image) or the root group's max width caps the window. */
    CHECK(mui_button_spacers(1, 1) == 2u && mui_button_spacers(0, 1) == 0u && mui_button_spacers(1, 0) == 0u, "icon-only button has no stretch spacers");
    CHECK(strstr(src, "mui_button_spacers(icon_only, 1)") != NULL && strstr(src, "HSpace(0)") != NULL, "mui_ui.c does not add the spacers");
    CHECK(strstr(src, "mui_make_button_icon(") != NULL && strstr(src, "icon_only && img != NULL ? NULL") != NULL, "dense rows are not icon-only in mui_ui.c");
}

/* I651: every MUI button is built with an icon. The keys below are the real button sources (transport actions for both play
 * states, extras, doors, toolbar labels, crumb); each must resolve to a baked Lucide body, the lookup counter must rise by
 * exactly that many, and mui_ui.c must build the image (Bodychunk with the theme palette) and the bigger padding. */
static void test_icon_buttons(void) {
    static char src[400000];
    sel_uim_frame fr;
    txt_theme th;
    uint32_t pal[12], before = mui_icon_resolved(), want = 0, i, b, st;
    size_t n;
    FILE* f;
    memset(&fr, 0, sizeof fr);
    for (st = 0; st < 2; st++) {
        fr.play = st ? SEL_PLAYING : SEL_STOPPED;
        for (b = 0; b < MUI_BTN_COUNT; b++) {
            const char* a = mui_transport_action(b, &fr);
            CHECK(a != NULL && mui_icon_body(mui_icon_id(a)) != NULL, "transport button %u state %u has no icon", (unsigned)b, (unsigned)st);
            want++;
        }
    }
    for (i = 0; mui_extra_action(i) != NULL; i++, want++) {
        CHECK(mui_icon_body(mui_icon_id(mui_extra_action(i))) != NULL, "extra %s has no icon", mui_extra_action(i));
    }
    for (i = 0; i < SEL_UIM_DOOR_COUNT; i++, want++) {
        CHECK(mui_icon_body(mui_icon_id(sel_uim_door_at(i)->page)) != NULL, "door %s has no icon", sel_uim_door_at(i)->page);
    }
    for (i = 0; i < MUI_TOOL_BTNS; i++, want++) {
        CHECK(mui_icon_body(mui_icon_id(mui_tool_labels[i])) != NULL, "tool %s has no icon", mui_tool_labels[i]);
    }
    CHECK(mui_icon_body(mui_icon_id("crumb")) != NULL, "crumb has no icon");
    want++;
    CHECK(mui_icon_resolved() - before == want, "icon lookups %u want %u", (unsigned)(mui_icon_resolved() - before), (unsigned)want);
    CHECK(mui_icon_id("no such button") < 0 && mui_icon_body(-1) == NULL, "unknown key has no icon");
    {
        const uint8_t* body = mui_icon_body(mui_icon_id("play"));
        size_t k, bytes = (size_t)mui_icon_body_px * (((mui_icon_body_px + 15) / 16) * 2) * 2, ink = 0;
        CHECK(mui_icon_body_px == MUI_ICON_PX, "baked edge %u", (unsigned)mui_icon_body_px);
        for (k = 0; k < bytes; k++) {
            ink += body[k] != 0;
        }
        CHECK(ink > 8, "play body is empty");
    }
    txt_theme_default(&th);
    mui_icon_palette(th.token, 1, pal);
    CHECK(pal[0] == (th.token[SEL_GUI_TOKEN_SURFACE_RAISED] >> 16 & 0xFFu) * 0x01010101u && pal[9] == (th.token[SEL_GUI_TOKEN_TEXT] >> 16 & 0xFFu) * 0x01010101u,
          "palette runs from the button face to the text colour");
    CHECK(MUI_BTN_PAD_X >= 8 && MUI_BTN_PAD_Y >= 4 && MUI_ICON_PX >= 24, "buttons are bigger than the 1 px text-only ones");
    f = fopen(MUI_UI_SOURCE, "rb");
    CHECK(f != NULL, "open %s", MUI_UI_SOURCE);
    if (f == NULL) {
        return;
    }
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "MUIC_Bodychunk") != NULL && strstr(src, "mui_icon_palette(") != NULL && strstr(src, "MUI_BTN_PAD_X") != NULL,
          "mui_ui.c builds no icon image");
    CHECK(strstr(src, "mui_make_button(mui_door_label(i), sel_uim_door_at(i)->page,") != NULL &&
              strstr(src, "mui_make_button_icon(mui_tool_labels[i], mui_tool_labels[i],") != NULL && strstr(src, "mui_make_button_w(s->crumb_lbl[k], \"crumb\",") != NULL &&
              strstr(src, "mui_make_button_i(s->btn_label[i], mui_transport_action(") != NULL && strstr(src, "mui_make_button_icon(mui_extra_label(i), mui_extra_action(i),") != NULL,
          "a button call site passes no icon key");
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
    } else if (strcmp(argv[1], "drop") == 0) {
        test_drop();
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
        test_row_cells();
    } else if (strcmp(argv[1], "theme") == 0) {
        test_theme();
    } else if (strcmp(argv[1], "style-menu") == 0) {
        test_style_menu();
    } else if (strcmp(argv[1], "style-mask") == 0) {
        test_style_mask();
    } else if (strcmp(argv[1], "list-pending") == 0) {
        test_list_pending();
    } else if (strcmp(argv[1], "list-backends") == 0) {
        test_list_backends();
    } else if (strcmp(argv[1], "doors") == 0) {
        test_doors();
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
        test_view_empty();
    } else if (strcmp(argv[1], "icon-buttons") == 0) {
        test_icon_buttons();
    } else if (strcmp(argv[1], "icon-rows") == 0) {
        test_icon_button_rows();
    } else if (strcmp(argv[1], "toolrows") == 0) {
        test_toolrows();
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
