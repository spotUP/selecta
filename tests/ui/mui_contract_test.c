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
    } else {
        return 2;
    }
    if (failures == 0) {
        puts("mui_contract_test: ok");
    }
    return failures != 0;
}
