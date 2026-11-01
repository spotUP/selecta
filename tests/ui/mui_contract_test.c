/* mui-wait-contract (MUI-P1-7), mui-registry (MUI-P1-6), mui-open-failure (MUI-P1-4), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_contract_test wait|registry|failure */
#include <selecta/plugin.h>
#include <selecta/registry.h>
#include <selecta/ui.h>
#include <selecta/uimodel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mui_aa.h"
#include "mui_icons.h"
#include "mui_route.h"
#include <selecta/library_item.h>
#include "mui_map.h"
#include "mui_aa.h"
#include "mui_theme.h"
#include "mui_pens.h"
#include "arexx.h"

#include <selecta/library_item.h>
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/keys.h>
#include <selecta/uimodel/pages.h>
#include <selecta/uimodel/route.h>
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
    s.arrive = 0;
    CHECK(mui_wait_step(&io, &s, 20) == 1, "a ReturnID is not 1 (handled)");
    s.arrive = 0x1000u; /* I727: Break while every turn has a ReturnID waiting (MP3 playback) */
    CHECK(mui_wait_step(&io, &s, 20) == -1, "break behind a ReturnID is not -1 (Break ignored during playback)");
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


/* I666: Selecta owns its screen, so the token colours exist there at exact values. The Amiga half (mui_ui.c) is compiled only on
 * the Amiga; this checks the plan it is built from (call counter, exact 32-bit components, DrawInfo text pen = soft grey token,
 * fits 16 colours for AGA) and that mui_ui.c opens the screen from that plan, obtains every colour exact and releases it. */
/* I676: the theme file (desktop format, txt_theme_parse) changes the MUI accent pen colour; ENV file first, --theme over it;
 * a missing or malformed file leaves the built-in value. dir: a writable directory for the two temp files. */
static void write_theme(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static void test_theme_file(const char* dir) {
    char env_path[512], arg_path[512], missing[512];
    txt_theme def, th;
    mui_pens_plan pl;
    uint32_t before = mui_pens_planned();
    snprintf(env_path, sizeof env_path, "%s/mui-theme-env.tmp", dir);
    snprintf(arg_path, sizeof arg_path, "%s/mui-theme-arg.tmp", dir);
    snprintf(missing, sizeof missing, "%s/mui-theme-missing.tmp", dir);
    remove(missing);
    txt_theme_default(&def);
    CHECK(def.token[SEL_GUI_TOKEN_ACCENT] != 0x112233u && def.token[SEL_GUI_TOKEN_ACCENT] != 0x445566u, "built-in accent is neither test colour");
    /* missing file: built-in value kept, nothing applied */
    th = def;
    CHECK(mui_theme_load(&th, missing, NULL) == 0 && mui_theme_load(&th, NULL, missing) == 0, "missing files apply nothing");
    CHECK(mui_pens_plan_for(th.token, &pl) == 1 && pl.rgb[MUI_PEN_ACCENT] == def.token[SEL_GUI_TOKEN_ACCENT], "missing file keeps the built-in accent pen");
    /* ENV file sets gui.accent: the accent pen colour follows */
    write_theme(env_path, "# amarchy\ngui.accent = 0x112233\n");
    th = def;
    CHECK(mui_theme_load(&th, env_path, NULL) == 1, "env file applied");
    CHECK(mui_pens_plan_for(th.token, &pl) == 1 && pl.rgb[MUI_PEN_ACCENT] == 0x112233u, "env gui.accent drives the accent pen, got %06x", (unsigned)pl.rgb[MUI_PEN_ACCENT]);
    CHECK(pl.rgb[MUI_PEN_BACKGROUND] == def.token[SEL_GUI_TOKEN_BACKGROUND], "untouched tokens keep the built-in value");
    /* --theme overrides the ENV file */
    write_theme(arg_path, "gui.accent = 0x445566\n");
    th = def;
    CHECK(mui_theme_load(&th, env_path, arg_path) == 2, "both files applied");
    CHECK(mui_pens_plan_for(th.token, &pl) == 1 && pl.rgb[MUI_PEN_ACCENT] == 0x445566u, "--theme wins over ENV, got %06x", (unsigned)pl.rgb[MUI_PEN_ACCENT]);
    /* malformed file: ignored whole, earlier valid lines do not leak in */
    write_theme(arg_path, "gui.accent = 0x778899\nnot a theme line\n");
    th = def;
    CHECK(mui_theme_load(&th, NULL, arg_path) == 0 && th.token[SEL_GUI_TOKEN_ACCENT] == def.token[SEL_GUI_TOKEN_ACCENT], "malformed file leaves the built-in tokens");
    CHECK(mui_pens_planned() - before >= 3, "pen plan ran (sentinel)");
    /* I676 (rig 2: accent stayed 3B82F6): the loaded tokens must reach the drawing classes. They read mui_theme_tokens(); a class
     * calling txt_theme_default itself draws the built-in colour whatever the file says. */
    th = def;
    mui_theme_load(&th, env_path, NULL);
    mui_theme_tokens_set(th.token);
    CHECK(mui_theme_tokens()[SEL_GUI_TOKEN_ACCENT] == 0x112233u, "token store carries the file accent, got %06x", (unsigned)mui_theme_tokens()[SEL_GUI_TOKEN_ACCENT]);
    {
        static const char* const files[] = {"mui_aa_draw.c", "mui_aabalance_class.c", "mui_aabtn_class.c", "mui_aapanel_class.c", "mui_aatext_class.c", "mui_aafield_class.c"};
        static char src[400000];
        size_t k, n;
        for (k = 0; k < sizeof files / sizeof files[0]; k++) {
            char path[600];
            FILE* sf;
            snprintf(path, sizeof path, "%s/%s", MUI_DIR, files[k]);
            sf = fopen(path, "rb");
            if (sf == NULL) {
                continue;
            }
            n = fread(src, 1, sizeof src - 1, sf);
            fclose(sf);
            src[n] = 0;
            CHECK(strstr(src, "txt_theme_default") == NULL, "%s builds its own built-in tokens instead of mui_theme_tokens()", files[k]);
        }
    }
    remove(env_path);
    remove(arg_path);
}

/* I676b: ARexx THEME. Drives arexx_dispatch (the port's entry) with the UI's handler; the window open reads the tokens through
 * mui_theme_resolve, so the accent pen after a THEME is what a reopened window gets. The ENV path is an Amiga path, so the test
 * uses a temp file as the "ENV" file. */
static int theme_exec_never(void* ctx, const char* line, char* reply, size_t cap, int* rc) {
    (void)ctx; (void)line; (void)reply; (void)cap;
    *rc = 1; /* THEME must not reach the engine executor */
    return 0;
}

static const char* g_theme_env;
static int theme_handler(void* ctx, const char* path) {
    (void)ctx;
    return mui_theme_request(g_theme_env, path);
}

static long theme_cmd(const char* line, char* reply, size_t cap) {
    arexx_result r;
    arexx_counters c = {0, 0};
    arexx_dispatch(line, 0, theme_exec_never, NULL, reply, cap, &r, &c);
    return r.result1;
}

static uint32_t theme_accent(void) {
    txt_theme th;
    mui_pens_plan pl;
    mui_theme_resolve(&th, g_theme_env, NULL);
    return mui_pens_plan_for(th.token, &pl) == 1 ? pl.rgb[MUI_PEN_ACCENT] : 0xFFFFFFFFu;
}

static void test_theme_live(const char* dir) {
    char env_path[512], new_path[512], bad_path[512], missing[512], line[600], reply[128];
    txt_theme def;
    uint32_t req0, accent0;
    snprintf(env_path, sizeof env_path, "%s/mui-live-env.tmp", dir);
    snprintf(new_path, sizeof new_path, "%s/mui-live-new.tmp", dir);
    snprintf(bad_path, sizeof bad_path, "%s/mui-live-bad.tmp", dir);
    snprintf(missing, sizeof missing, "%s/mui-live-missing.tmp", dir);
    remove(missing);
    g_theme_env = env_path;
    txt_theme_default(&def);
    write_theme(env_path, "gui.accent = 0x112233\n");
    write_theme(new_path, "gui.accent = 0x445566\n");
    write_theme(bad_path, "gui.accent = 0x778899\nnot a theme line\n");

    /* no handler registered: refused */
    arexx_set_theme_handler(NULL, NULL);
    CHECK(theme_cmd("THEME", reply, sizeof reply) == AREXX_RC_FAILED, "THEME without a UI handler returns rc 10");
    arexx_set_theme_handler(theme_handler, NULL);

    accent0 = theme_accent();
    CHECK(accent0 == 0x112233u, "start state: ENV accent, got %06x", (unsigned)accent0);
    req0 = mui_theme_requests();

    snprintf(line, sizeof line, "theme \"%s\"", new_path);
    CHECK(theme_cmd(line, reply, sizeof reply) == AREXX_RC_OK, "THEME <path> accepted, rc 0");
    CHECK(mui_theme_requests() == req0 + 1u, "the UI request ran (sentinel)");
    CHECK(theme_accent() == 0x445566u, "THEME <path> changes the accent pen, got %06x", (unsigned)theme_accent());

    /* malformed and missing: rc 10, tokens unchanged */
    snprintf(line, sizeof line, "THEME %s", bad_path);
    CHECK(theme_cmd(line, reply, sizeof reply) == AREXX_RC_FAILED, "malformed theme returns rc 10");
    CHECK(strcmp(reply, "theme_refused") == 0, "refusal text, got '%s'", reply);
    CHECK(theme_accent() == 0x445566u, "malformed theme leaves the tokens unchanged, got %06x", (unsigned)theme_accent());
    snprintf(line, sizeof line, "THEME %s", missing);
    CHECK(theme_cmd(line, reply, sizeof reply) == AREXX_RC_FAILED, "missing theme returns rc 10");
    CHECK(theme_accent() == 0x445566u, "missing theme leaves the tokens unchanged");
    CHECK(mui_theme_requests() == req0 + 1u, "refused requests are not counted");

    /* bare THEME re-reads the ENV file */
    write_theme(env_path, "gui.accent = 0x0A0B0C\n");
    CHECK(theme_cmd("THEME", reply, sizeof reply) == AREXX_RC_OK, "bare THEME accepted");
    CHECK(theme_accent() == 0x0A0B0Cu, "bare THEME re-reads ENV, got %06x", (unsigned)theme_accent());
    write_theme(env_path, "garbage\n");
    CHECK(theme_cmd("THEME", reply, sizeof reply) == AREXX_RC_FAILED, "malformed ENV file returns rc 10");
    CHECK(theme_accent() == 0x0A0B0Cu || theme_accent() == def.token[SEL_GUI_TOKEN_ACCENT], "malformed ENV does not apply");

    arexx_set_theme_handler(NULL, NULL);
    remove(env_path);
    remove(new_path);
    remove(bad_path);
}

/* I676c: the live THEME. The handler is the plugin's own SEL_UI_THEME hook (what main.c registers), driven through arexx_dispatch.
 * Pens change in the same call that answers rc 0, and no window is closed or opened: the hook's source holds no open or close, and
 * main.c holds no reopen. Fake ops record what the repaint received. */
static uint32_t g_live_pal_calls, g_live_cls_calls, g_live_pal_accent;
static void live_palette(void* ctx, const uint32_t* tokens) {
    (void)ctx;
    g_live_pal_calls++;
    g_live_pal_accent = tokens[SEL_GUI_TOKEN_ACCENT];
}
static void live_classes(void* ctx) {
    (void)ctx;
    g_live_cls_calls++;
}

static void test_theme_live_repaint(const char* dir) {
    char env_path[512], new_path[512], line[600], reply[128], src[262144];
    const sel_tag* hooks = (const sel_tag*)sel_tag_find(sel_plugin_entry_mui_test(1u), SEL_PLUGIN_TAGS, 0);
    int (*hook)(void*, const char*) = (int (*)(void*, const char*))sel_tag_find(hooks, SEL_UI_THEME, 0);
    mui_theme_live_ops ops;
    uint32_t live0;
    FILE* f;
    size_t n;
    char* body;
    snprintf(env_path, sizeof env_path, "%s/mui-liverp-env.tmp", dir);
    snprintf(new_path, sizeof new_path, "%s/mui-liverp-new.tmp", dir);
    write_theme(env_path, "gui.accent = 0x112233\n");
    write_theme(new_path, "gui.accent = 0x445566\n");
    CHECK(hook != NULL, "the plugin offers SEL_UI_THEME");
    g_theme_env = env_path;

    /* the apply path: new tokens reach the palette once, the classes repaint once */
    ops.palette = live_palette;
    ops.classes = live_classes;
    ops.ctx = NULL;
    live0 = mui_theme_live_applied();
    CHECK(mui_theme_request(env_path, new_path) == 0, "request accepted");
    CHECK(mui_theme_apply_live(&ops, env_path, NULL) == 0, "live apply ran");
    CHECK(g_live_pal_calls == 1 && g_live_cls_calls == 1, "palette %u and classes %u calls", (unsigned)g_live_pal_calls, (unsigned)g_live_cls_calls);
    CHECK(g_live_pal_accent == 0x445566u, "the palette got the new accent, got %06x", (unsigned)g_live_pal_accent);
    CHECK(mui_theme_tokens()[SEL_GUI_TOKEN_ACCENT] == 0x445566u, "the token store (what aa_setup and aa_retheme_all copy) holds the new accent");
    CHECK(mui_theme_live_applied() == live0 + 1u, "live sentinel");
    CHECK(mui_theme_apply_live(NULL, env_path, NULL) == -1, "no ops: refused");

    /* through the ARexx port: rc 0 comes with a live apply, a refusal with none */
    arexx_set_theme_handler(hook, NULL);
    live0 = mui_theme_live_applied();
    snprintf(line, sizeof line, "THEME \"%s\"", new_path);
    CHECK(theme_cmd(line, reply, sizeof reply) == AREXX_RC_OK, "THEME accepted through the hook");
    CHECK(mui_theme_live_applied() == live0 + 1u, "the THEME ran the live apply, not a reopen");
    write_theme(env_path, "garbage\n");
    CHECK(theme_cmd("THEME", reply, sizeof reply) == AREXX_RC_FAILED, "bad file refused");
    CHECK(mui_theme_live_applied() == live0 + 1u, "a refused THEME repaints nothing");
    arexx_set_theme_handler(NULL, NULL);

    /* no close or open on the THEME path (call-count sentinel by source): the hook, the live ops and main.c */
    f = fopen(MUI_UI_SOURCE, "rb");
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    body = strstr(src, "static void mui_live_palette(");
    CHECK(body != NULL && strstr(body, "SetRGB32") != NULL, "the live palette reloads the held pens in place");
    body = strstr(src, "static int mui_theme_hook(void* state, const char* path) {");
    CHECK(body != NULL, "hook found");
    if (body != NULL) {
        char* end = strstr(body, "\n}\n");
        if (end != NULL) {
            *end = 0;
        }
        CHECK(strstr(body, "MUIA_Window_Open") == NULL && strstr(body, "mui_close") == NULL && strstr(body, "mui_open") == NULL &&
                  strstr(body, "ui_switch") == NULL, "the THEME hook neither closes nor opens the window");
    }
    f = fopen(MUI_DIR "/../../../apps/selecta/main.c", "rb");
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(n > 0 && strstr(src, "g_theme_reopen") == NULL, "main.c has no THEME reopen path");
    remove(env_path);
    remove(new_path);
}

static void test_pens(void) {
    txt_theme th;
    mui_pens_plan pl;
    uint32_t pens_rgb[MUI_DRI_PENS];
    uint32_t before = mui_pens_planned();
    int i;
    FILE* f;
    char src[400000];
    size_t n;
    txt_theme_default(&th);
    CHECK(mui_pens_plan_for(th.token, &pl) == 1 && mui_pens_planned() - before == 1, "pen plan built once (sentinel)");
    CHECK(mui_pens_plan_for(NULL, &pl) == 0 && mui_pens_plan_for(th.token, NULL) == 0, "bad input refused");
    CHECK(MUI_PEN_TOTAL <= 16, "palette of %d fits an AGA 16 colour screen", (int)MUI_PEN_TOTAL);
    CHECK(pl.rgb[MUI_PEN_BACKGROUND] == th.token[SEL_GUI_TOKEN_BACKGROUND] && pl.rgb[MUI_PEN_SURFACE] == th.token[SEL_GUI_TOKEN_SURFACE] &&
              pl.rgb[MUI_PEN_RAISED] == th.token[SEL_GUI_TOKEN_SURFACE_RAISED] && pl.rgb[MUI_PEN_PANEL] == th.token[SEL_GUI_TOKEN_PANEL] &&
              pl.rgb[MUI_PEN_TEXT] == th.token[SEL_GUI_TOKEN_TEXT] && pl.rgb[MUI_PEN_ACCENT] == th.token[SEL_GUI_TOKEN_ACCENT],
          "every slot holds its token colour");
    CHECK(pl.rgb[MUI_PEN_TEXT] != 0xFFFFFFu && pl.rgb[MUI_PEN_BACKGROUND] != 0x000000u, "text is not white and the background is not black");
    for (i = 0; i < MUI_PEN_TOTAL; i++) {
        CHECK(pl.rgb32[i][0] == ((pl.rgb[i] >> 16) & 0xFFu) * 0x01010101u && pl.rgb32[i][1] == ((pl.rgb[i] >> 8) & 0xFFu) * 0x01010101u &&
                  pl.rgb32[i][2] == (pl.rgb[i] & 0xFFu) * 0x01010101u,
              "slot %d 32-bit components are the 8-bit token replicated (exact, no rounding)", i);
    }
    CHECK(pl.dri_pens[MUI_DRI_PENS] == -1 && pl.dri_pens[2] == MUI_PEN_TEXT, "SA_Pens array ends with -1 and TEXTPEN is the text slot");
    for (i = 0; i < MUI_DRI_PENS; i++) {
        CHECK(pl.dri_pens[i] >= 0 && pl.dri_pens[i] < MUI_PEN_COUNT, "DrawInfo pen %d points at a palette slot", i);
        pens_rgb[i] = pl.rgb[pl.dri_pens[i]];
    }
    /* I663: the Intuition frame parts (window title bar, borders, resize gadget, divider shading) are drawn from the screen
     * DrawInfo pens, so each must resolve to a dark token (every channel below 0x60) and the bar and frame text to the TEXT
     * token. DrawInfo pen order: DETAIL 0, BLOCK 1, TEXT 2, SHINE 3, SHADOW 4, FILL 5, FILLTEXT 6, BACKGROUND 7, HILIGHTTEXT 8,
     * BARDETAIL 9, BARBLOCK 10, BARTRIM 11. Rig 2 pixel read (2026-10-29) found no light or blue-grey frame colour. */
    {
        static const int dark_pens[] = {1, 3, 4, 5, 7, 10, 11};
        static const char* const dark_names[] = {"BLOCK", "SHINE", "SHADOW", "FILL", "BACKGROUND", "BARBLOCK (title bar)", "BARTRIM (frame trim)"};
        for (i = 0; i < 7; i++) {
            uint32_t c = pens_rgb[dark_pens[i]];
            CHECK(((c >> 16) & 0xFFu) < 0x60u && ((c >> 8) & 0xFFu) < 0x60u && (c & 0xFFu) < 0x60u, "DrawInfo pen %s is a dark token, got %06x",
                  dark_names[i], (unsigned)c);
        }
        CHECK(pl.dri_pens[10] == MUI_PEN_PANEL && pl.dri_pens[11] == MUI_PEN_BACKGROUND && pl.dri_pens[9] == MUI_PEN_TEXT,
              "title bar: BARBLOCK is PANEL, BARTRIM is BACKGROUND, BARDETAIL is TEXT");
    }
    /* I672 mui-pens-muted: secondary labels draw in TEXT_MUTED (A0A0AC), primary text stays TEXT (C8C8D0). The muted slot is a
     * DrawInfo pen (0..7) so the MUI text escape can select it; TEXTPEN stays the TEXT slot. */
    CHECK(pl.rgb[MUI_PEN_TEXT_MUTED] == 0xA0A0ACu && pl.rgb[MUI_PEN_TEXT_MUTED] == th.token[SEL_GUI_TOKEN_TEXT_MUTED], "pen table holds A0A0AC for TEXT_MUTED");
    CHECK(pl.rgb[MUI_PEN_TEXT] == 0xC8C8D0u, "TEXT stays C8C8D0");
    /* I667c: 13 token slots (design section 4 AGA pen table), icon pens 13-15, 16 pens in all. */
    CHECK(MUI_PEN_COUNT == 13 && MUI_PEN_TOTAL == 16 && MUI_PEN_ICON_FIRST == 13, "13 token slots then icon pens 13-15, got %d", (int)MUI_PEN_COUNT);
    CHECK(MUI_PEN_TEXT_MUTED == 7 && MUI_PEN_TEXT_DISABLED == 8 && MUI_PEN_TRACK == 9 && MUI_PEN_SURFACE_HOVER == 10 &&
              MUI_PEN_SURFACE_PRESSED == 11 && MUI_PEN_FOCUS_RING == 12,
          "slots 7-12 are muted, disabled, track, hover, pressed, focus ring");
    CHECK(pl.rgb[MUI_PEN_TEXT_DISABLED] == th.token[SEL_GUI_TOKEN_TEXT_DISABLED] && pl.rgb[MUI_PEN_TRACK] == th.token[SEL_GUI_TOKEN_TRACK] &&
              pl.rgb[MUI_PEN_SURFACE_HOVER] == th.token[SEL_GUI_TOKEN_SURFACE_HOVER] &&
              pl.rgb[MUI_PEN_SURFACE_PRESSED] == th.token[SEL_GUI_TOKEN_SURFACE_PRESSED] &&
              pl.rgb[MUI_PEN_FOCUS_RING] == th.token[SEL_GUI_TOKEN_FOCUS_RING] && pl.rgb[MUI_PEN_TEXT_DISABLED] == 0x62626Eu &&
              pl.rgb[MUI_PEN_TRACK] == 0x3A3A44u && pl.rgb[MUI_PEN_SURFACE_HOVER] == 0x34343Eu && pl.rgb[MUI_PEN_SURFACE_PRESSED] == 0x3E3E4Au &&
              pl.rgb[MUI_PEN_FOCUS_RING] == 0x93C5FDu,
          "slots 8-12 hold their token values");
    {
        uint32_t ip[12];
        mui_icon_palette(th.token, 1, ip);
        for (i = 0; i < MUI_PEN_ICON_COUNT; i++) {
            CHECK(pl.rgb32[MUI_PEN_ICON_FIRST + i][0] == ip[3 + i * 3] && pl.rgb32[MUI_PEN_ICON_FIRST + i][1] == ip[4 + i * 3] &&
                      pl.rgb32[MUI_PEN_ICON_FIRST + i][2] == ip[5 + i * 3],
                  "icon pen %d is mui_icon_palette pen %d", 13 + i, 1 + i);
        }
    }
    CHECK(mui_pens_aga_slot(SEL_GUI_TOKEN_ACCENT_TEXT) == MUI_PEN_TEXT && mui_pens_aga_slot(SEL_GUI_TOKEN_HOVER_LIGHT) == MUI_PEN_TEXT &&
              mui_pens_aga_slot(SEL_GUI_TOKEN_HEADER_TINT) == MUI_PEN_PANEL && mui_pens_aga_slot(SEL_GUI_TOKEN_TRACK) == MUI_PEN_TRACK &&
              mui_pens_aga_slot(SEL_GUI_TOKEN_FOCUS_RING) == MUI_PEN_FOCUS_RING,
          "AGA fallback: ACCENT_TEXT and HOVER_LIGHT -> TEXT, HEADER_TINT -> PANEL, held tokens keep their slot");
    CHECK(mui_pens_label_slot(MUI_LBL_ARTIST) == MUI_PEN_TEXT_MUTED && mui_pens_label_slot(MUI_LBL_CLOCK) == MUI_PEN_TEXT_MUTED &&
              mui_pens_label_slot(MUI_LBL_SECTION) == MUI_PEN_TEXT_MUTED && mui_pens_label_slot(MUI_LBL_TITLE) == MUI_PEN_TEXT,
          "player artist line maps to the muted pen, the title keeps TEXT");
    CHECK(mui_pens_dri_index(&pl, MUI_PEN_TEXT_MUTED) == 0 && mui_pens_dri_index(&pl, MUI_PEN_TEXT) == 2, "muted escape digit 0, text digit 2");
    f = fopen(MUI_UI_SOURCE, "rb");
    CHECK(f != NULL, "open %s", MUI_UI_SOURCE);
    if (f == NULL) {
        return;
    }
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "mui_make_text(s, MUI_ATTR_ARTIST, \"\", MUI_LBL_ARTIST)") != NULL && strstr(src, "mui_pens_label_tone(label)") != NULL,
          "mui_ui.c routes the artist label through the label table");
    CHECK(strstr(src, "mui_pens_plan_for(") != NULL, "mui_ui.c builds the palette through mui_pens_plan_for");
    CHECK(strstr(src, "SA_Pens") != NULL && strstr(src, "SA_Colors32") != NULL && strstr(src, "SA_PubName") != NULL, "mui_ui.c opens its own public screen with the plan palette and DrawInfo pens");
    /* I667c sentinel: the screen palette loader and the held-pen loop run over the full table (13 token slots plus 3 icon pens). */
    CHECK(strstr(src, "ULONG pal[2 + MUI_PEN_TOTAL * 3]") != NULL && strstr(src, "(ULONG)MUI_PEN_TOTAL << 16") != NULL &&
              strstr(src, "g_pen_held[MUI_PEN_TOTAL]") != NULL && strstr(src, "pal[1 + MUI_PEN_TOTAL * 3] = 0") != NULL,
          "mui_ui.c loads the 16 entry table through SA_Colors32 and holds every pen");
    CHECK(strstr(src, "PRECISION_EXACT") != NULL && strstr(src, "ObtainBestPen(") != NULL, "mui_ui.c obtains every token colour at exact precision");
    CHECK(strstr(src, "ReleasePen(") != NULL && strstr(src, "CloseScreen(") != NULL, "mui_ui.c releases the pens and closes the screen");
    CHECK(strstr(src, "MUIA_Window_PublicScreen") != NULL, "windows open on the Selecta screen");
}

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
    /* I667e: AAPanel roles map to the tokens (panel PANEL 141418, surface SURFACE, raised SURFACE_RAISED), radius RADIUS_LARGE 8 (tier S 4), spacing 8. */
    CHECK(mui_aa_panel_rgb(th.token, MUI_AA_PANEL_PANEL) == th.token[SEL_GUI_TOKEN_PANEL] && mui_aa_panel_rgb(th.token, MUI_AA_PANEL_PANEL) == 0x141418u, "panel role is PANEL");
    CHECK(mui_aa_panel_rgb(th.token, MUI_AA_PANEL_SURFACE) == th.token[SEL_GUI_TOKEN_SURFACE], "surface role is SURFACE");
    CHECK(mui_aa_panel_rgb(th.token, MUI_AA_PANEL_RAISED) == th.token[SEL_GUI_TOKEN_SURFACE_RAISED], "raised role is SURFACE_RAISED");
    CHECK(mui_aa_panel_radius(th.token, 13) == 8 && mui_aa_panel_radius(th.token, 0) == 4, "panel radius 8, tier S 4");
    CHECK(mui_aa_panel_spacing(th.token) == 8, "panel spacing 8");
    mui_theme_bg_spec(0x141418u, want, sizeof want);
    CHECK(strcmp(want, "2:14141414,14141414,18181818") == 0, "background spec %s", want);
    CHECK(mui_theme_look_for(MUI_TH_COUNT, th.token, &lk) == 0 && mui_theme_look_for(MUI_TH_WINDOW, NULL, &lk) == 0, "bad input refused");
    CHECK(mui_theme_readable(th.token[SEL_GUI_TOKEN_TEXT], th.token[SEL_GUI_TOKEN_BACKGROUND]) == 1, "light text on the dark theme is readable");
    CHECK(mui_theme_readable(0x000000u, th.token[SEL_GUI_TOKEN_BACKGROUND]) == 0, "black MUI text on the dark theme is not readable: theme stays off");
    /* I660: one decision for the whole look, over the screen pens MUI can draw text with (DrawInfo pens). A Workbench-style
     * palette (black text pen, white shine pen) gets the white pen; a screen with only dark pens gets -1 (look off everywhere). */
    {
        static const uint32_t wb[8] = {0x000000u, 0xFFFFFFu, 0x000000u, 0xFFFFFFu, 0x000000u, 0x6688BBu, 0x000000u, 0xFFFFFFu};
        static const uint32_t dark[8] = {0x000000u, 0x000000u, 0x000000u, 0x202020u, 0x000000u, 0x101010u, 0x000000u, 0x181818u};
        int pen = mui_theme_pick_pen(wb, 8, th.token);
        CHECK(pen == 1 || pen == 3 || pen == 7, "light pen picked on a Workbench palette, got %d", pen);
        CHECK(mui_theme_pick_pen(dark, 8, th.token) == -1, "no readable pen: look off for every role");
        /* I665: the gate measures WCAG contrast on linear (sRGB-decoded) luminance. The rig's text pen C8C8D0 on the
         * button face 2A2A32 is about 8.5:1; without linearisation it read 3.9 and switched every role off. */
        {
            static const uint32_t rig[4] = {0x000000u, 0xC8C8D0u, 0x2A2A32u, 0x121214u};
            CHECK(mui_theme_readable(0xC8C8D0u, 0x2A2A32u) == 1, "C8C8D0 text on 2A2A32 button face is readable");
            CHECK(mui_theme_pick_pen(rig, 4, th.token) == 1, "rig palette picks the C8C8D0 text pen, got %d", mui_theme_pick_pen(rig, 4, th.token));
        }
        CHECK(mui_theme_pick_pen(NULL, 8, th.token) == -1 && mui_theme_pick_pen(wb, 8, NULL) == -1, "bad input refused");
    }
    f = fopen(MUI_UI_SOURCE, "rb");
    CHECK(f != NULL, "open %s", MUI_UI_SOURCE);
    if (f == NULL) {
        return;
    }
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "mui_theme_pick_pen(") != NULL, "mui_ui.c decides the look once through mui_theme_pick_pen");
    CHECK(strstr(src, "mui_theme_readable(text, g_th[i]") == NULL, "mui_ui.c gates roles one by one against the TEXTPEN colour");
    for (role = 0; role < MUI_TH_COUNT; role++) {
        char key[64];
        if (role == MUI_TH_BUTTON) {
            continue; /* exemption: buttons are AAButton objects that draw from mui_aa_button_rgb, no stock frame role (design 3.1) */
        }
        if (role == MUI_TH_PANEL) {
            /* exemption (I667e, design 3.8): panel groups are AAPanel objects that draw from mui_aa_panel_rgb; mui-panel-source proves no TH_TAGS panel is left */
            CHECK(strstr(src, "mui_aapanel(MUI_AA_PANEL_PANEL") != NULL, "mui_ui.c builds no AAPanel panel group");
            continue;
        }
        if (role == MUI_TH_FIELD) {
            /* exemption (I667h, design 3.4): string gadgets are AAField objects that draw from mui_aa_field_plan; mui-field-source proves no stock string is left */
            CHECK(strstr(src, "mui_aafield_new(") != NULL, "mui_ui.c builds no AAField string");
            continue;
        }
        if (role == MUI_TH_LIST) {
            /* exemption (I667i2, design 3.6): lists are AAList objects that draw from tokens; mui-list-source proves no stock list is left */
            CHECK(strstr(src, "mui_aalist_new(") != NULL, "mui_ui.c builds no AAList");
            continue;
        }
        if (role == MUI_TH_STATUS) {
            /* exemption (I740, design 3.7): status lines are plain AAText labels with no frame and no field box; mui-status-face proves it */
            CHECK(strstr(src, "sp.face") == NULL && strstr(src, "framed") == NULL, "mui_aatext still has a framed or face look");
            continue;
        }
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

/* mui-weights (MUI-P2-1, I615d design 8.5): horizontal weights equal the region widths and sum to the window width, on both tiers; the vertical
 * weights (player + main + suggestion + top bar) sum to the height; the top bar slot follows the layout's top bar. */
static void test_weights(void) {
    static const uint32_t widths[] = {1280, 1024, 800, 640};
    static const uint32_t tiers[] = {SEL_UIM_TIER_DEFAULT, SEL_UIM_TIER_BITMAP};
    uint32_t i, t;
    for (t = 0; t < 2; t++) {
        for (i = 0; i < 4; i++) {
            mui_layout_map_t m;
            sel_uim_layout l;
            uint32_t sum, s;
            mui_layout_map(&m, widths[i], 720, tiers[t]);
            sel_uim_layout_compute_tier(&l, widths[i], 720, tiers[t]);
            sum = m.weight_w[MUI_SLOT_SIDEBAR] + m.weight_w[MUI_SLOT_MAIN] + m.weight_w[MUI_SLOT_DETAIL];
            CHECK(sum == widths[i], "tier %u width %u: body weights sum %u", tiers[t], widths[i], sum);
            for (s = 0; s < l.count; s++) {
                const char* n = l.regions[s].name;
                uint32_t slot = strcmp(n, "sidebar") == 0 ? MUI_SLOT_SIDEBAR : strcmp(n, "main") == 0 ? MUI_SLOT_MAIN : strcmp(n, "detail") == 0 ? MUI_SLOT_DETAIL : MUI_SLOT_COUNT;
                if (slot != MUI_SLOT_COUNT) {
                    CHECK(m.weight_w[slot] == l.regions[s].w, "tier %u width %u: %s weight %u != region %u", tiers[t], widths[i], n, m.weight_w[slot], l.regions[s].w);
                }
            }
            CHECK(m.weight_h[MUI_SLOT_PLAYER] + m.weight_h[MUI_SLOT_MAIN] + m.weight_h[MUI_SLOT_SUGGESTION] + m.weight_h[MUI_SLOT_TOPBAR] == 720,
                  "tier %u width %u: vertical weights", tiers[t], widths[i]);
            CHECK(m.show[MUI_SLOT_TOPBAR] == (l.topbar.h > 0u), "tier %u width %u: top bar slot shown %u, layout top bar %u", tiers[t], widths[i], m.show[MUI_SLOT_TOPBAR], l.topbar.h);
            if (tiers[t] == SEL_UIM_TIER_BITMAP) {
                CHECK(m.show[MUI_SLOT_TOPBAR] == 0, "bitmap tier width %u has no top bar", widths[i]);
            }
            for (s = 0; s < 3; s++) { /* I792: a region the shared layout shows has a MUI slot shown, and one it hides has none */
                static const char* const rn[3] = {"sidebar", "main", "detail"};
                static const uint32_t rs[3] = {MUI_SLOT_SIDEBAR, MUI_SLOT_MAIN, MUI_SLOT_DETAIL};
                uint32_t k, in_layout = 0;
                for (k = 0; k < l.count; k++) {
                    in_layout |= strcmp(l.regions[k].name, rn[s]) == 0 && l.regions[k].w > 0u;
                }
                CHECK(m.show[rs[s]] == in_layout, "tier %u width %u: layout shows %s %u, MUI slot shown %u", tiers[t], widths[i], rn[s], in_layout, m.show[rs[s]]);
            }
            CHECK(m.balance_sidebar_main == (m.show[MUI_SLOT_SIDEBAR] && m.show[MUI_SLOT_MAIN]), "balance sidebar");
            CHECK(m.balance_main_detail == (widths[i] >= 1200), "balance detail at %u", widths[i]);
        }
    }
}

/* mui-topbar (I615d, design 8.5): the real mui_layout_map entry on the DEFAULT tier shows the top bar slot with the layout's height, the bitmap
 * tier does not; Home resolves to the Home door's line and Browse to the rich UI's Browse page lines; the sidebar drops Home and Search only
 * with the top bar. mui_ui.c wires the tier to the screen and the slot to the window. */
static void test_topbar(void) {
    mui_layout_map_t m;
    sel_uim_layout l;
    static char src[400000];
    size_t n;
    FILE* f;
    uint32_t i, shown_top = 0, shown_plain = 0;
    mui_layout_map(&m, 1280, 720, SEL_UIM_TIER_DEFAULT);
    sel_uim_layout_compute_tier(&l, 1280, 720, SEL_UIM_TIER_DEFAULT);
    CHECK(l.topbar.h == SEL_UIM_TOPBAR_H && m.show[MUI_SLOT_TOPBAR] == 1 && m.weight_h[MUI_SLOT_TOPBAR] == SEL_UIM_TOPBAR_H && m.weight_w[MUI_SLOT_TOPBAR] == 1280,
          "default tier: top bar slot shown %u weight_h %u", m.show[MUI_SLOT_TOPBAR], m.weight_h[MUI_SLOT_TOPBAR]);
    mui_layout_map(&m, 1280, 720, SEL_UIM_TIER_BITMAP);
    CHECK(m.show[MUI_SLOT_TOPBAR] == 0 && m.weight_h[MUI_SLOT_TOPBAR] == 0, "bitmap tier: no top bar slot");
    CHECK(mui_door_line(0) != NULL && strcmp(mui_door_line(0), "VIEW_SOURCE home") == 0, "Home sends the Home door line");
    CHECK(MUI_TOPBAR_BROWSE_LINES == 3u && strcmp(mui_topbar_browse_lines[0], "FORMATS_SHOW") == 0 && strcmp(mui_topbar_browse_lines[1], "COMPOSERS_SHOW") == 0 &&
              strcmp(mui_topbar_browse_lines[2], "SOURCES") == 0, "Browse sends the rich UI's PG_BROWSE lines");
    for (i = 0; i < mui_door_count(); i++) {
        shown_top += (uint32_t)sel_uim_door_shown(i, 1);
        shown_plain += (uint32_t)sel_uim_door_shown(i, 0);
    }
    CHECK(shown_top == 5u && shown_plain == 7u && !sel_uim_door_shown(0, 1) && !sel_uim_door_shown(1, 1), "doors with the top bar %u, without %u", shown_top, shown_plain);
    f = fopen(MUI_UI_SOURCE, "r");
    CHECK(f != NULL, "mui_ui.c readable");
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(strstr(src, "g_screen_rtg ? SEL_UIM_TIER_DEFAULT : SEL_UIM_TIER_BITMAP") != NULL && strstr(src, "s->slot[MUI_SLOT_TOPBAR] = mui_topbar_group(s)") != NULL &&
              strstr(src, "mui_sidebar_group(s, g_screen_rtg)") != NULL && strstr(src, "in.top_bar = top_bar;") != NULL, "mui_ui.c passes the screen tier, builds the top bar slot and filters the doors through the sidebar model");
}

/* mui-relayout (MUI-P2-2): the shown set per breakpoint; the slot count is fixed, and sel_uim_breakpoint_for_tier (bitmap tier, I674: 640 keeps a rail) decides. */
static void test_relayout(void) {
    static const uint32_t widths[] = {1280, 1024, 800, 640};
    static const uint8_t want[4][MUI_SLOT_COUNT] = {{0, 1, 1, 1, 1, 0}, {0, 1, 1, 0, 1, 0}, {0, 1, 1, 0, 1, 0}, {0, 1, 1, 0, 1, 0}};
    mui_layout_map_t m;
    uint32_t i, s, flips = 99;
    mui_layout_map(&m, 1280, 720, SEL_UIM_TIER_BITMAP);
    for (i = 0; i < 4; i++) {
        uint32_t changed = mui_layout_relayout(&m, widths[i], 720, SEL_UIM_TIER_BITMAP, &flips);
        CHECK(m.breakpoint == sel_uim_breakpoint_for_tier(widths[i], SEL_UIM_TIER_BITMAP), "breakpoint at %u", widths[i]);
        CHECK(changed == (i != 0 && sel_uim_breakpoint_for_tier(widths[i], SEL_UIM_TIER_BITMAP) != sel_uim_breakpoint_for_tier(widths[i - 1], SEL_UIM_TIER_BITMAP)), "changed flag at %u", widths[i]);
        for (s = 0; s < MUI_SLOT_COUNT; s++) {
            CHECK(m.show[s] == want[i][s], "width %u slot %u show %u", widths[i], s, m.show[s]);
        }
    }
    mui_layout_map(&m, 1280, 720, SEL_UIM_TIER_BITMAP);
    /* The three-column edge is SEL_UIM_BP_THREE_COLUMN_MIN (1100, DUI-P1-11): 1100 itself is still three-column. */
    CHECK(mui_layout_relayout(&m, SEL_UIM_BP_THREE_COLUMN_MIN, 720, SEL_UIM_TIER_BITMAP, &flips) == 0 && flips == 0, "1100 stays three-column (flips %u)", flips);
    CHECK(mui_layout_relayout(&m, SEL_UIM_BP_THREE_COLUMN_MIN - 1u, 720, SEL_UIM_TIER_BITMAP, &flips) == 1 && flips == 1, "one below the edge hides only detail (flips %u)", flips);
    CHECK(mui_layout_relayout(&m, 1000, 720, SEL_UIM_TIER_BITMAP, &flips) == 0 && flips == 0, "same breakpoint changes no visibility");
}

/* mui-relayout-open (I656): the open sequence (default map, width 0, first real size 792) never leaves the narrow-window
 * suggestion shown at icon-sidebar width, and the suggestion object is created hidden: an object hidden only after the
 * window opened stays listed in the screen's object tree with a stale box. */
static void test_open_no_suggestion(void) {
    mui_layout_map_t m;
    uint32_t flips;
    char line[512];
    int found = 0;
    FILE* f = fopen(MUI_UI_SOURCE, "r");
    mui_layout_map(&m, 1280, 720, SEL_UIM_TIER_BITMAP);
    m.width = 0;
    m.breakpoint = SEL_UIM_BP_COUNT;
    mui_layout_relayout(&m, 792, 576, SEL_UIM_TIER_BITMAP, &flips);
    CHECK(m.show[MUI_SLOT_SUGGESTION] == 0, "792 wide shows no suggestion");
    mui_layout_relayout(&m, 560, 576, SEL_UIM_TIER_BITMAP, &flips);
    CHECK(m.show[MUI_SLOT_SUGGESTION] == 1, "560 wide (mini) shows the suggestion");
    /* I674: the 640 wide own screen (window 600-699) keeps a rail and shows no "Window is narrow" hint. */
    mui_layout_relayout(&m, 600, 576, SEL_UIM_TIER_BITMAP, &flips);
    CHECK(m.show[MUI_SLOT_SUGGESTION] == 0 && m.show[MUI_SLOT_SIDEBAR] == 1, "600 wide shows a rail, no suggestion");
    mui_layout_relayout(&m, 640, 512, SEL_UIM_TIER_BITMAP, &flips);
    CHECK(m.show[MUI_SLOT_SUGGESTION] == 0, "640 wide shows no suggestion");
    CHECK(f != NULL, "mui_ui.c readable");
    while (f != NULL && fgets(line, sizeof line, f) != NULL) {
        if (strstr(line, "slot[MUI_SLOT_SUGGESTION] = mui_aatext(") != NULL) {
            found = 1;
            CHECK(strstr(line, "MUI_AA_ALIGN_LEFT, 1)") != NULL, "suggestion object is created hidden");
        }
    }
    CHECK(found, "suggestion object creation found");
    if (f != NULL) {
        fclose(f);
    }
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

/* mui-seek-row (I667f3): the three time labels of the seek row are sized to a sample string, every other text keeps the
 * flexible default; a sample is wide enough for h:mm:ss and its remaining form carries the minus sign. */
static void test_seek_row(void) {
    uint32_t a;
    for (a = 0; a < MUI_ATTR_COUNT; a++) {
        const char* f = mui_attr_fixwidth(a);
        int label = a == MUI_ATTR_CLOCK || a == MUI_ATTR_REMAINING || a == MUI_ATTR_LENGTH;
        CHECK((f != NULL) == label, "attr %u fixed width presence %d", a, f != NULL);
    }
    CHECK(strlen(mui_attr_fixwidth(MUI_ATTR_CLOCK)) >= strlen("0:00:00"), "clock sample too narrow");
    CHECK(strlen(mui_attr_fixwidth(MUI_ATTR_REMAINING)) == strlen(mui_attr_fixwidth(MUI_ATTR_LENGTH)) + 1, "remaining sample lacks the minus sign");
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
    char f[128];
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
            CHECK(strncmp(f, "W=40 DELTA=16 BAR,W=25", 22) == 0, "title and artist carry weights: '%s' at %u", f, bp);
        }
    }
}

/* MUI-P3-4 (I819: the format cell is the shared label, long name plus extension): the album, length and format cells come from the snapshot, not from the (title, artist) engine row. */
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
    CHECK(strcmp(x.format, "Protracker (mod)") == 0 && strcmp(x.length, "03:05") == 0 && strcmp(x.album, "Evoke") == 0, "playing row: '%s' '%s' '%s'", x.format, x.length, x.album);
    r.id = 6;
    mui_row_extra_fill(&st, &r, NULL, &x);
    CHECK(strcmp(x.format, "Fasttracker 2 (xm)") == 0 && strcmp(x.length, "--:--") == 0 && x.album[0] == '\0', "queued row: '%s' '%s' '%s'", x.format, x.length, x.album);
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

/* MUI-P3-1 (I716): a double click acts on the row the list reported as picked, not on the List cursor a refresh may have
 * moved since; the DBL branch of mui_ui.c reads the picked row from the list and decides through mui_list_dbl_row. */
static void test_list_dbl_row(void) {
    static char src[400000];
    uint32_t c0 = mui_list_dbl_row_calls;
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    const char* dbl;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(mui_list_dbl_row(1, 0) == 1, "picked row 1 wins over a cursor moved back to row 0");
    CHECK(mui_list_dbl_row(2, 5) == 2, "picked wins over any cursor");
    CHECK(mui_list_dbl_row(-1, 3) == 3, "nothing picked: the cursor row");
    CHECK(mui_list_dbl_row(0, 0) == 0, "row 0 picked stays row 0");
    CHECK(mui_list_dbl_row_calls == c0 + 4, "the decision ran %u times, expected 4", (unsigned)(mui_list_dbl_row_calls - c0));
    /* I724: the knob. Off by default; on, the race (cursor reset to row 0 by a refresh) is injected and the clicked row still wins. */
    CHECK(!mui_list_dbl_race_on(NULL) && !mui_list_dbl_race_on("") && !mui_list_dbl_race_on("0") && mui_list_dbl_race_on("1") &&
              mui_list_dbl_race_on("on"),
          "MUIDblRace knob: unset, empty and 0 are off, anything else is on");
    CHECK(mui_list_dbl_row(4, 0) == 4, "knob path: the refresh reset the List cursor to row 0 (..), the picked row 4 is still the one that plays");
    dbl = strstr(src, "mui_dbl_race_inject((mui_state*)ctx);");
    CHECK(dbl != NULL && strstr(dbl, "mui_control_fire((mui_state*)ctx, id);") == dbl + strlen("mui_dbl_race_inject((mui_state*)ctx);\n        }\n        "),
          "the injection sits directly before mui_control_fire for CTL_LIST_DBL");
    CHECK(strstr(src, "GetVar((CONST_STRPTR)\"Selecta/MUIDblRace\"") != NULL && strstr(src, "\"dbl_race_injected\"") != NULL &&
              strstr(src, "s->p4[P4_DBLRACE]++") != NULL,
          "the knob is read once at open and the sentinel dbl_race_injected is counted");
    dbl = strstr(src, "if (id == CTL_LIST_DBL) {");
    CHECK(dbl != NULL && strstr(dbl, "GetAttr(AAL_Picked, s->list") != NULL && strstr(dbl, "mui_list_dbl_row(") != NULL &&
              strstr(dbl, "mui_list_dbl_row(") < strstr(dbl, "VIEW_ACTIVATE"),
          "the DBL branch of mui_ui.c reads AAL_Picked and decides through mui_list_dbl_row before VIEW_ACTIVATE");
    n = 0;
    f = fopen(MUI_DIR "/mui_aalist_class.c", "rb");
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(strstr(src, "d->picked = r;") != NULL && strstr(src, "case AAL_Picked: *msg->opg_Storage = (ULONG)d->picked") != NULL,
          "the AAList class stores the picked row and answers AAL_Picked");
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

/* I792, the parity guard (drift doc 2026-10-30-mui-mac-drift section 4): every page of the shared page table has a MUI slot or a written
 * exemption, and every page a route target can open is in the table. A page added to uimodel/pages.h with no entry in mui_page_slot is
 * kind MUI_PAGE_GAP and fails here. */
static void mui_route_targets(void) {
    uint32_t pg, t, k;
    for (pg = 0; pg < (uint32_t)SEL_UIM_PG_COUNT; pg++) {
        const mui_page_slot_t* ps = mui_page_slot(pg);
        CHECK(ps != NULL && (ps->kind == MUI_PAGE_MAIN_VIEW || ps->kind == MUI_PAGE_GROUP || (ps->kind == MUI_PAGE_EXEMPT && ps->exempt != NULL && ps->exempt[0] != '\0')),
              "page '%s' has neither a MUI slot nor an exemption", sel_uim_page_at(pg)->name);
        CHECK(sel_uim_page_of_route(sel_uim_page_route(pg)) == pg, "page '%s' route id does not round trip", sel_uim_page_at(pg)->name);
    }
    CHECK(mui_page_slot((uint32_t)SEL_UIM_PG_COUNT) == NULL, "past the page table");
    for (t = 0; t < (uint32_t)SEL_ROUTE_TARGETS; t++) {
        for (k = 0; k < (uint32_t)SEL_UIM_PG_COUNT; k++) {
            sel_route r;
            if (sel_route_resolve((sel_route_target)t, k, &r) && r.page != 0u) {
                uint32_t id = sel_uim_page_of_route(r.page);
                const mui_page_slot_t* ps = mui_page_slot(id);
                CHECK(sel_uim_page_route(id) == r.page || (uint32_t)SEL_ROUTE_PAGE_OTHER_BASE + id == r.page, "route target %u item %u opens route page %u that is not in the page table", (unsigned)t, (unsigned)k, (unsigned)r.page);
                CHECK(ps != NULL && ps->kind != MUI_PAGE_GAP, "route target %u item %u opens page '%s' with no MUI slot or exemption", (unsigned)t, (unsigned)k, sel_uim_page_at(id)->name);
            }
        }
    }
}

/* I735: the Visualizer and Pattern pages are main-area panes. The page entry (mui_pane_key, the Mac UI's default key table) opens each
 * as a main view; the `opens` counter proves the entry ran; the detail slot that held them before is hidden under 1100 px while the
 * main slot is not. */
static void test_panes(void) {
    static mui_menu_item items[512];
    mui_mainpane mp;
    mui_layout_map_t lm;
    uint32_t i, n, door_pg, width;
    const uint32_t pages[2] = {SEL_UIM_PG_VIZ, SEL_UIM_PG_TRACKER};
    const uint32_t panes[2] = {MUI_PANE_VIZ, MUI_PANE_TRACKER};
    const char keys[2] = {'v', 't'};
    memset(&mp, 0, sizeof mp);
    for (i = 0; i < 2; i++) {
        const mui_page_slot_t* ps = mui_page_slot(pages[i]);
        CHECK(ps != NULL && ps->kind == MUI_PAGE_MAIN_VIEW && ps->exempt == NULL, "page '%s' is not a main view without exemption", sel_uim_page_at(pages[i])->name);
        CHECK(mui_pane_shortcut(panes[i]) == keys[i], "pane %u shortcut is '%c', the Mac key is '%c'", (unsigned)panes[i], mui_pane_shortcut(panes[i]), keys[i]);
        CHECK(mui_pane_title(panes[i]) != NULL, "pane %u has no menu title", (unsigned)panes[i]);
        CHECK(mui_pane_key(&mp, (uint32_t)keys[i], 0u) == panes[i] && mp.pane == panes[i], "key '%c' does not open pane %u", keys[i], (unsigned)panes[i]);
        CHECK(mp.opens[panes[i]] == 1u, "pane %u entry ran %u times, not once", (unsigned)panes[i], (unsigned)mp.opens[panes[i]]);
        CHECK(mui_pane_open(&mp, pages[i]) == panes[i] && mp.opens[panes[i]] == 2u, "page entry for pane %u did not count", (unsigned)panes[i]);
    }
    /* a door page (the list) and a key that opens no page */
    door_pg = sel_uim_page_by_name(sel_uim_door_at(0)->page);
    CHECK(mui_pane_open(&mp, door_pg) == MUI_PANE_LIST && mp.pane == MUI_PANE_LIST && mp.opens[MUI_PANE_LIST] == 1u, "a door does not return to the list pane");
    CHECK(mui_pane_page(MUI_PANE_VIZ) == (uint32_t)SEL_UIM_PG_VIZ && mui_pane_page(MUI_PANE_TRACKER) == (uint32_t)SEL_UIM_PG_TRACKER && mui_pane_page(MUI_PANE_LIST) == (uint32_t)SEL_UIM_PG_HOME, "pane to page map");
    mp.pane = MUI_PANE_VIZ;
    CHECK(mui_pane_key(&mp, (uint32_t)'j', 0u) == MUI_PANE_COUNT && mp.pane == MUI_PANE_VIZ, "an unrelated key moved the pane");
    CHECK(mui_pane_key(&mp, (uint32_t)'v', TXT_MOD_CTRL) == MUI_PANE_COUNT, "Ctrl+V opened a page");
    CHECK(mui_pane_of_action("@viz") == MUI_PANE_VIZ && mui_pane_of_action("@pattern") == MUI_PANE_TRACKER && mui_pane_of_action("@home") == MUI_PANE_COUNT && mui_pane_of_action(NULL) == MUI_PANE_COUNT, "action to pane map");
    /* the menu shortcuts of the other items leave 'v' and 't' to the panes */
    n = mui_menu_build_for(items, 512, mui_style_mask(0));
    for (i = 0; i < n && i < 512u; i++) {
        char c = items[i].shortcut;
        CHECK(c == '\0' || (c != 'v' && c != 't' && c != 'V' && c != 'T'), "menu item '%s' takes the pane shortcut %c", items[i].item, c);
    }
    /* every width: main slot shown, detail slot hidden below 1100 px (where the panes used to be unreachable) */
    for (width = 320; width <= 1920; width += 160) {
        mui_layout_map(&lm, width, 720, SEL_UIM_TIER_DEFAULT);
        CHECK(lm.show[MUI_SLOT_MAIN] != 0u, "main slot hidden at width %u", (unsigned)width);
        CHECK(width >= 1100u || lm.show[MUI_SLOT_DETAIL] == 0u, "detail slot shown at width %u", (unsigned)width);
    }
}

/* MUI-P4-0: seven doors from the shared table, each with its label; the five main-view pages carry a line the engine accepts. */
static void test_doors(void) {
    uint32_t i, lines = 0;
    CHECK(mui_door_count() == SEL_UIM_DOOR_COUNT, "%u doors", (unsigned)SEL_UIM_DOOR_COUNT);
    for (i = 0; i < mui_door_count(); i++) {
        const sel_uim_door* dr = sel_uim_door_at(i);
        uint32_t pg = sel_uim_page_by_name(dr->page);
        const mui_page_slot_t* ps = mui_page_slot(pg);
        CHECK(mui_door_label(i) != NULL && strcmp(mui_door_label(i), dr->label) == 0, "door %u label", (unsigned)i);
        CHECK(pg < (uint32_t)SEL_UIM_PG_COUNT && ps != NULL, "door %u page '%s' is not in the page table", (unsigned)i, dr->page);
        /* I792: a door whose page the MUI shows in the main view needs the engine line; a group page or an exemption sends none */
        CHECK(ps == NULL || ps->kind != MUI_PAGE_MAIN_VIEW || mui_door_line(i) != NULL, "door %u ('%s'): main-view page '%s' has no engine line", (unsigned)i, dr->label, dr->page);
        lines += mui_door_line(i) != NULL;
    }
    mui_route_targets();
    CHECK(lines == 5u && strcmp(mui_door_line(1), "VIEW_SOURCE search") == 0 && mui_door_line(5) == NULL, "five main-view lines");
    CHECK(mui_door_label(7) == NULL && mui_door_line(7) == NULL, "past the table");
    /* I615d (design 8.4): with the top bar (DEFAULT tier) Home and Search leave the sidebar, the bitmap tier keeps all seven. */
    for (i = 0, lines = 0; i < 7u; i++) {
        lines += (uint32_t)sel_uim_door_shown(i, 1);
    }
    CHECK(lines == 5u && !sel_uim_door_shown(0, 1) && !sel_uim_door_shown(1, 1) && sel_uim_door_shown(2, 1), "five doors with the top bar, not Home or Search");
    for (i = 0, lines = 0; i < 7u; i++) {
        lines += (uint32_t)sel_uim_door_shown(i, 0);
    }
    CHECK(lines == 7u, "seven doors on the bitmap tier");
}

/* I793 (drift doc S3 to S9): the MUI Your Library sidebar has the Mac sidebar's parts: header, chips, item rows, empty state and
 * create card, with Browse Folders hidden through the uimodel. The sentinel mui_sidebar_builds() proves the model ran, and the
 * mui_ui.c sidebar group is checked to build from it. */
static char* ui_source(void);
static void test_sidebar(void) {
    sel_engine_library_item it[3];
    mui_sidebar_in in;
    mui_sidebar_model m;
    uint32_t i, builds = mui_sidebar_builds();
    int folders = 0;
    const char* src = ui_source();
    memset(it, 0, sizeof it);
    memset(&in, 0, sizeof in);
    it[0].kind = SEL_LIB_ITEM_LIKED; it[0].pinned = 1; snprintf(it[0].name, sizeof it[0].name, "Liked Songs"); it[0].songs = 12;
    it[1].kind = SEL_LIB_ITEM_PLAYLIST; it[1].id = 7; snprintf(it[1].name, sizeof it[1].name, "Road"); snprintf(it[1].sub, sizeof it[1].sub, "You");
    snprintf(it[1].creator, sizeof it[1].creator, "You"); it[1].songs = 3;
    it[2].kind = SEL_LIB_ITEM_ALBUM; snprintf(it[2].name, sizeof it[2].name, "Chip Tunes"); snprintf(it[2].sub, sizeof it[2].sub, "Virgill"); it[2].songs = 9;
    in.items = it; in.item_n = 3; in.filter = "";
    /* header (S4) and the doors (S3): Browse Folders is gone, the shared rule says so for the desktop too */
    mui_sidebar_build(&in, &m);
    CHECK(mui_sidebar_builds() == builds + 1u, "sentinel %u", (unsigned)mui_sidebar_builds());
    CHECK(strcmp(m.title, "Your Library") == 0 && strcmp(m.create, "Create Playlist") == 0 && strcmp(m.expand, "Expand Library") == 0, "header '%s' '%s' '%s'", m.title, m.create, m.expand);
    for (i = 0; i < m.door_n; i++) {
        folders += strcmp(sel_uim_door_at(m.door[i])->page, "folders") == 0;
    }
    CHECK(m.door_n == 6u && folders == 0 && !sel_uim_door_shown_in(2, 0, 1) && sel_uim_door_shown_in(2, 0, 0), "doors %u folders %d", (unsigned)m.door_n, folders);
    /* chips (S5): the six kinds, all shown while none is selected; a selected chip hides the others and offers the clear X */
    CHECK(strcmp(m.chip[0], "Playlists") == 0 && strcmp(m.chip[5], "Local Files") == 0 && m.chip_shown[0] && m.chip_shown[5] && !m.chip_clear, "chips");
    /* rows (S6): the Mac subtitle formatter, kind mark, and the Expand columns (S7) */
    CHECK(m.row_n == 3u && strcmp(m.row[0].title, "Liked Songs") == 0 && strcmp(m.row[0].subtitle, "Playlist \xe2\x80\xa2 12 songs") == 0 && strcmp(m.row[2].subtitle, "Album \xe2\x80\xa2 Virgill") == 0,
          "rows %u '%s' '%s'", (unsigned)m.row_n, m.row[0].subtitle, m.row[2].subtitle);
    CHECK(m.cols == 1u && strcmp(m.row[1].type, "Playlist") == 0 && strcmp(m.row[2].mark, "A") == 0, "mark/type");
    in.wide = 1;
    mui_sidebar_build(&in, &m);
    CHECK(m.cols == 4u && strcmp(m.expand, "Collapse Library") == 0, "expanded: %u columns '%s'", (unsigned)m.cols, m.expand);
    in.wide = 0;
    /* a chip filters by kind and hides the other chips; a kind without rows shows the empty text (S8) */
    in.chip = 2; /* Albums */
    mui_sidebar_build(&in, &m);
    CHECK(m.row_n == 1u && strcmp(m.row[0].title, "Chip Tunes") == 0 && m.chip_shown[1] && !m.chip_shown[0] && m.chip_clear && m.empty[0] == '\0' && !m.card, "album chip");
    in.chip = 3; /* Artists: none in the list */
    mui_sidebar_build(&in, &m);
    CHECK(m.row_n == 0u && strcmp(m.empty, "Nothing here yet.") == 0, "empty '%s'", m.empty);
    /* the create card (S9): only without a playlist row, on All or Playlists, no filter */
    in.chip = 0; in.item_n = 1;
    mui_sidebar_build(&in, &m);
    CHECK(m.card && strcmp(m.card_title, "Create your first playlist") == 0 && strcmp(m.card_button, "Create playlist") == 0 && m.row_n == 1u, "card");
    in.item_n = 3;
    mui_sidebar_build(&in, &m);
    CHECK(!m.card, "a playlist exists: no card");
    in.filter = "Chip";
    in.item_n = 1;
    mui_sidebar_build(&in, &m);
    CHECK(!m.card, "a filter hides the card");
    /* the MUI path: mui_sidebar_group builds its objects from the model, and the buttons route to the shared actions */
    CHECK(src != NULL, "read mui_ui.c");
    if (src != NULL) {
        const char* g = strstr(src, "static Object* mui_sidebar_dyn(const mui_sidebar_model* m) {");
        const char* gs = g != NULL ? strstr(g, "static Object* mui_sidebar_group(") : NULL;
        const char* end = gs != NULL ? strstr(gs, "\n}\n") : NULL;
        char body[12000];
        snprintf(body, sizeof body, "%.*s", g != NULL && end != NULL ? (int)(end - g < 11990 ? end - g : 11990) : 0, g != NULL ? g : "");
        CHECK(strstr(body, "mui_sidebar_build(") != NULL && strstr(body, "m.title") != NULL && strstr(body, "m->chip") != NULL && strstr(body, "m->row") != NULL &&
              strstr(body, "m->empty") != NULL && strstr(body, "m->card") != NULL && strstr(body, "m.door") != NULL, "mui_sidebar_group does not build from the model");
        CHECK(strstr(src, "mui_viewbtn_fire(1,") != NULL && strstr(src, "CTL_SIDE_BASE") != NULL, "sidebar buttons are not routed");
    }
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
        if (mui_tool_row_chars(r) == 0u || mui_tool_row_chars(r) > 72u) {
            fprintf(stderr, "FAIL tool row %u holds %u label characters (1..72; the buttons are icon-only, the pixel width is checked by icon-rows)\n", (unsigned)r, (unsigned)mui_tool_row_chars(r));
            failures++;
        }
    }
}

/* Regression (I671): on a 640x256 AGA screen MUI refused the window (MUIA_Window_Open read back 0, requester "the MUI
 * window could not open") because the three-column layout was applied only after the open, with a fixed 1280x720 size,
 * so the open measured the sidebar and detail panels too. The open must apply the layout of the screen it opens on first,
 * and that layout is the bitmap tier one: since I674 the 640 wide screen keeps the icon rail (sidebar shown) and hides the detail panel. */
static void test_layout_before_open(void) {
    static char src[400000];
    mui_layout_map_t m;
    const char *pick, *open_at;
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    mui_layout_map(&m, 640u, 256u, SEL_UIM_TIER_BITMAP);
    CHECK(m.show[MUI_SLOT_MAIN] && m.show[MUI_SLOT_PLAYER] && m.show[MUI_SLOT_SIDEBAR] && !m.show[MUI_SLOT_DETAIL],
          "640x256 layout shows sidebar %u detail %u", (unsigned)m.show[MUI_SLOT_SIDEBAR], (unsigned)m.show[MUI_SLOT_DETAIL]);
    pick = strstr(src, "    mui_layout_for_screen(s);\n    set(s->win, MUIA_Window_Open, TRUE);");
    open_at = strstr(src, "set(s->win, MUIA_Window_Open, TRUE);");
    CHECK(pick != NULL && open_at != NULL && pick < open_at, "mui_open does not apply the screen layout before MUIA_Window_Open");
    CHECK(strstr(src, "mui_layout_map(&s->lay, 1280, 720)") == NULL, "mui_open still applies a fixed 1280x720 layout");
}

/* Regression (I671): on rig 2 (AGA, Workbench PAL hires 640x256, mode 0x29000) MUI 3.8 refused the window (MUIA_Window_Open
 * read back 0): the mini layout is taller than the screen. The height model must say so (taller than 256, within 512), the
 * plan must open the own screen in the laced variant of a native mode that is too short (height doubled, mode | LACE), keep
 * a laced or tall enough mode, keep an RTG mode at depth 8, and mui_screen_open must apply the plan (sentinel wiring). */
static void test_screen_plan(void) {
    mui_screen_plan_t p;
    uint32_t need = mui_layout_min_height();
    uint32_t before = mui_screen_plans();
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(mui_button_height() == (uint32_t)mui_model_icon_px() + 2u * MUI_BTN_PAD_Y + MUI_BTN_FRAME, "button height %u", (unsigned)mui_button_height());
    CHECK(need > 256u && need <= 512u, "mini layout minimum height %u must be taller than 256 and fit 512", (unsigned)need);
    CHECK(mui_screen_plan(0x29000u, 640u, 256u, 0, need, &p) == 1 && p.laced && p.mode == 0x29004u && p.width == 640u &&
              p.height == 512u && p.depth == 4u,
          "AGA 640x256 plan: laced %u mode %08x %ux%u depth %u", (unsigned)p.laced, (unsigned)p.mode, (unsigned)p.width,
          (unsigned)p.height, (unsigned)p.depth);
    CHECK(mui_screen_plan(0x29004u, 640u, 512u, 0, need, &p) == 0 && !p.laced && p.mode == 0x29004u && p.height == 512u,
          "laced 640x512 plan changed: mode %08x height %u", (unsigned)p.mode, (unsigned)p.height);
    CHECK(mui_screen_plan(0x29000u, 640u, 256u, 0, 200u, &p) == 0 && p.mode == 0x29000u, "a fitting layout still laces");
    CHECK(mui_screen_plan(0x50041303u, 800u, 600u, 1, need, &p) == 0 && !p.laced && p.depth == 8u && p.mode == 0x50041303u,
          "RTG 800x600 plan: laced %u depth %u", (unsigned)p.laced, (unsigned)p.depth);
    CHECK(mui_screen_plan(MUI_INVALID_MODE, 0u, 0u, 0, need, &p) == 0 && p.mode == MUI_INVALID_MODE, "invalid mode laced");
    CHECK(mui_screen_plans() == before + 5u && mui_screen_laced() >= 1u, "plan sentinels %u %u", (unsigned)mui_screen_plans(),
          (unsigned)mui_screen_laced());
    CHECK(strstr(src, "mui_screen_plan((uint32_t)mode") != NULL && strstr(src, "SA_DisplayID, mode") != NULL &&
              strstr(src, "mode = plan.mode;") != NULL && strstr(src, "SA_Depth, (ULONG)plan.depth") != NULL,
          "mui_screen_open does not open the own screen from mui_screen_plan");
    CHECK(strstr(src, "ModeNotAvailable(plan.mode)") != NULL, "mui_screen_open does not check the laced mode is available");
    /* I667n: rig 2 uaegfx Workbench 0x50041303 800x600x24 has DIPF_IS_FOREIGN clear; the depth-4 plan held all 16 pens on RTG. */
    CHECK(mui_screen_is_rtg(0, 0, 24u) && mui_screen_is_rtg(0, 1, 16u) && mui_screen_is_rtg(1, 1, 8u) && mui_screen_is_rtg(0, 0, 8u) &&
              !mui_screen_is_rtg(0, 1, 8u) && !mui_screen_is_rtg(0, 1, 4u),
          "RTG detection: a uaegfx bitmap without DIPF_IS_FOREIGN is RTG, a standard planar one is not");
    CHECK(strstr(src, "foreign = mui_screen_is_rtg(") != NULL && strstr(src, "BMF_STANDARD") != NULL,
          "mui_screen_open does not decide RTG through mui_screen_is_rtg with the Workbench bitmap");
    CHECK(strstr(src, "_minheight(s->root)") != NULL, "mui_open does not report the measured window minimum");
}

/* I675 (D-M4-3): on the own SELECTA screen the main window is a borderless backdrop window without drag bar, depth, size or
 * close gadget; on Workbench (compact mode) it keeps normal borders. mui_open must take the attributes from
 * mui_window_attrs (sentinel counter + source wiring), decided by the same g_scr test TH_SCR uses. */
static void test_window_borderless(void) {
    mui_window_attrs_t a;
    uint32_t before = mui_window_attrs_calls();
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    mui_window_attrs(1, &a);
    CHECK(a.borderless == 1 && a.backdrop == 1 && a.dragbar == 0 && a.depthgadget == 0 && a.sizegadget == 0 && a.closegadget == 0 && a.titled == 0,
          "own screen is not an untitled borderless backdrop window");
    mui_window_attrs(0, &a);
    CHECK(a.borderless == 0 && a.backdrop == 0 && a.dragbar == 1 && a.depthgadget == 1 && a.sizegadget == 1 && a.closegadget == 1 && a.titled == 1,
          "Workbench window lost its borders or title");
    CHECK(mui_window_attrs_calls() == before + 2u, "attrs sentinel %u", (unsigned)mui_window_attrs_calls());
    CHECK(strstr(src, "mui_window_attrs(g_scr != NULL, &wattr)") != NULL, "mui_open does not choose attributes by own screen");
    CHECK(strstr(src, "MUIA_Window_Borderless, (ULONG)wattr.borderless") != NULL &&
              strstr(src, "MUIA_Window_Backdrop, (ULONG)wattr.backdrop") != NULL &&
              strstr(src, "MUIA_Window_DragBar, (ULONG)wattr.dragbar") != NULL &&
              strstr(src, "MUIA_Window_DepthGadget, (ULONG)wattr.depthgadget") != NULL &&
              strstr(src, "MUIA_Window_SizeGadget, (ULONG)wattr.sizegadget") != NULL &&
              strstr(src, "MUIA_Window_CloseGadget, (ULONG)wattr.closegadget") != NULL,
          "the main WindowObject does not apply the attributes");
    /* Rig 2 2026-10-29: Intuition draws the screen title bar in front of backdrop windows, so the SELECTA screen must
     * open without it or the borderless window still sits under a title bar. */
    /* Rig 2 2026-10-29 (second look): the band stayed with SA_ShowTitle FALSE. The probe read BorderTop 9 on the WFLG_BORDERLESS
     * window: Intuition gives any titled window a title bar, so the main window takes its title from the attributes. */
    CHECK(strstr(src, "MUIA_Window_Title, wattr.titled ? (ULONG) \"Selecta\" : (ULONG)NULL") != NULL,
          "the main window keeps its title (and Intuition's title bar) on the own screen");
    CHECK(strstr(src, "SA_ShowTitle, FALSE") != NULL, "the SELECTA screen shows its title bar over the backdrop window");
}

/* I730: every dialog window (Settings, Help, the Commands palette) takes the one shared attribute path. Behaviour: on the own
 * screen borderless, untitled, no gadgets, not a backdrop, own header row; on Workbench the normal borders. Structure: each
 * `<name>_win` that is a dialog is built by mui_dialog_window (which calls mui_dialog_attrs), and no other code creates a MUIC_Window
 * object except the main window (WindowObject, I675) and the cycle popup (mui_aacycle_class.c, not a dialog). A dialog that builds
 * its own MUIC_Window bypasses the path and turns this red. */
static void test_dialog_windows(void) {
    mui_window_attrs_t a;
    uint32_t before = mui_dialog_attrs_calls();
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    const char *fn, *end, *p;
    uint32_t windows = 0, via_helper = 0;
    static const char* const dialogs[] = {"s->help_win = mui_dialog_window(", "s->pal_win = mui_dialog_window(", "s->set_win = mui_dialog_window("};
    size_t i;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    mui_dialog_attrs(1, &a);
    CHECK(a.borderless == 1 && a.backdrop == 0 && a.dragbar == 0 && a.depthgadget == 0 && a.sizegadget == 0 && a.closegadget == 0 && a.titled == 0 && a.header == 1,
          "own screen dialog is not borderless, untitled, gadget-free with its own header");
    mui_dialog_attrs(0, &a);
    CHECK(a.borderless == 0 && a.backdrop == 0 && a.dragbar == 1 && a.depthgadget == 1 && a.sizegadget == 1 && a.closegadget == 1 && a.titled == 1 && a.header == 0,
          "Workbench dialog lost its normal borders");
    CHECK(mui_dialog_attrs_calls() == before + 2u, "dialog attrs sentinel %u", (unsigned)mui_dialog_attrs_calls());
    mui_window_attrs(1, &a);
    CHECK(a.header == 0 && a.backdrop == 1, "the main window must keep backdrop and no header");
    fn = strstr(src, "static Object* mui_dialog_window(");
    CHECK(fn != NULL, "mui_dialog_window is gone");
    end = fn != NULL ? strstr(fn, "\n}\n") : NULL;
    CHECK(end != NULL && strstr(fn, "mui_dialog_attrs(g_scr != NULL, &da)") != NULL && strstr(fn, "mui_dialog_attrs(g_scr != NULL, &da)") < end,
          "mui_dialog_window does not take its attributes from mui_dialog_attrs by own screen");
    CHECK(end != NULL && strstr(fn, "mui_aabtn_new_key(&sp, 27UL)") != NULL && strstr(fn, "mui_aabtn_new_key(&sp, 27UL)") < end, "the dialog close button does not take Esc");
    CHECK(end != NULL && strstr(fn, "MUIA_Window_Borderless, (ULONG)da.borderless") != NULL && strstr(fn, "MUIA_Window_CloseGadget, (ULONG)da.closegadget") != NULL,
          "mui_dialog_window does not apply the attributes");
    CHECK(end != NULL && strstr(fn, "MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered") != NULL, "dialogs are not centred");
    for (i = 0; i < sizeof dialogs / sizeof dialogs[0]; i++) {
        CHECK(strstr(src, dialogs[i]) != NULL, "dialog not built by mui_dialog_window: %s", dialogs[i]);
    }
    for (p = src; (p = strstr(p, "MUIC_Window")) != NULL; p++) {
        windows++;
        if (fn != NULL && end != NULL && p > fn && p < end) {
            via_helper++;
        }
    }
    CHECK(windows == via_helper, "%u MUIC_Window objects outside mui_dialog_window: a dialog bypasses the shared path", (unsigned)(windows - via_helper));
    CHECK(strstr(src, "_win = MUI_NewObject(") == NULL, "a secondary window is built outside mui_dialog_window");
}

/* I731 (plan 2026-10-31-I731-mui-flat-tooltips.md section 6): the flat tooltip. Plan numbers, placement and the hover state machine run on
 * the host; the Amiga module is proven wired by its source text. The sentinel proves the case ran the real mui_aa_tip_plan. */
static int tip_file(const char* name, char* buf, size_t cap) {
    char path[600];
    FILE* f;
    size_t n;
    snprintf(path, sizeof path, "%s/%s", MUI_DIR, name);
    f = fopen(path, "rb");
    if (f == NULL) {
        buf[0] = 0;
        return 0;
    }
    n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return 1;
}

static void test_tooltip_flat(void) {
    const uint32_t* tok = mui_theme_tokens();
    uint32_t before = mui_aa_tip_calls();
    mui_aa_tip_t p, s;
    int x = 0, y = 0, act, st, side;
    static char src[400000];
    static const char* const classes[] = {"mui_aabtn_class.c", "mui_aaslider_class.c", "mui_aafield_class.c"};
    static const char* const need[] = {"aatip_super_set(", "aatip_event(", "AATIP_M_FIRE", "aatip_drop("};
    static const char* const tip_need[] = {"mui_aa_tip_plan(", "mui_aa_tip_place(", "mui_aa_tip_step(", "aa_round_fill(", "aa_text(", "WA_Borderless", "WA_Activate, FALSE", "MUIIHNF_TIMER"};
    static const char* const tip_ban[] = {"MUIM_CreateBubble", "MUIA_Frame", "aa_round_frame("};
    const char *fn, *end;
    size_t k, j;

    mui_aa_tip_plan(tok, 14, 80, 16, &p);
    mui_aa_tip_plan(tok, 0, 80, 8, &s);
    CHECK(mui_aa_tip_calls() == before + 2u, "tip plan sentinel %u", (unsigned)mui_aa_tip_calls());
    CHECK(p.face == tok[SEL_GUI_TOKEN_SURFACE_PRESSED] && p.text == tok[SEL_GUI_TOKEN_TEXT] && p.corner == tok[SEL_GUI_TOKEN_SURFACE], "tip colours are not the desktop tokens");
    CHECK(p.pad_x == (int)tok[SEL_GUI_TOKEN_SPACING] && p.w == 80 + 2 * p.pad_x && p.h == 16 + 2 * p.pad_y, "tip box size w=%d h=%d pad_x=%d", p.w, p.h, p.pad_x);
    CHECK(p.radius == (tok[SEL_GUI_TOKEN_RADIUS_SMALL] < (uint32_t)(p.h / 2) ? (int)tok[SEL_GUI_TOKEN_RADIUS_SMALL] : p.h / 2), "tip radius %d", p.radius);
    CHECK(s.pad_x == 6 && s.pad_y == 3 && s.radius == 4 && s.gap == 2, "tier S tip numbers %d/%d/%d/%d", s.pad_x, s.pad_y, s.radius, s.gap);

    side = mui_aa_tip_place(300, 200, 40, 20, 80, 24, 800, 600, 4, MUI_AA_TIP_ABOVE, &x, &y);
    CHECK(side == MUI_AA_TIP_ABOVE && x == 300 + (40 - 80) / 2 && y + 24 + 4 == 200, "above placement side=%d x=%d y=%d", side, x, y);
    side = mui_aa_tip_place(300, 0, 40, 20, 80, 24, 800, 600, 4, MUI_AA_TIP_ABOVE, &x, &y);
    CHECK(side == MUI_AA_TIP_BELOW && y == 24, "anchor at the top must flip below, side=%d y=%d", side, y);
    side = mui_aa_tip_place(0, 100, 40, 20, 80, 24, 800, 600, 4, MUI_AA_TIP_RIGHT, &x, &y);
    CHECK(side == MUI_AA_TIP_RIGHT && x == 44, "door tip beside the door, side=%d x=%d", side, x);
    side = mui_aa_tip_place(780, 300, 20, 20, 80, 24, 800, 600, 4, MUI_AA_TIP_ABOVE, &x, &y);
    CHECK(x == 800 - 80, "right edge clamp x=%d", x);

    CHECK(MUI_AA_TIP_DELAY_MS == 600, "tip delay is not the desktop 600 ms");
    act = 0;
    st = mui_aa_tip_step(MUI_AA_TIP_IDLE, MUI_AA_TIP_EV_ENTER, &act);
    CHECK(st == MUI_AA_TIP_PENDING && (act & MUI_AA_TIP_DO_ARM), "enter does not arm");
    act = 0;
    st = mui_aa_tip_step(st, MUI_AA_TIP_EV_TIMER, &act);
    CHECK(st == MUI_AA_TIP_SHOWN && (act & MUI_AA_TIP_DO_SHOW), "timer does not show");
    act = 0;
    st = mui_aa_tip_step(st, MUI_AA_TIP_EV_PRESS, &act);
    CHECK(st == MUI_AA_TIP_MUTED && (act & MUI_AA_TIP_DO_HIDE), "press does not mute and hide");
    act = 0;
    st = mui_aa_tip_step(st, MUI_AA_TIP_EV_ENTER, &act);
    CHECK(st == MUI_AA_TIP_MUTED && !(act & MUI_AA_TIP_DO_ARM), "enter while muted must stay muted");
    act = 0;
    st = mui_aa_tip_step(st, MUI_AA_TIP_EV_LEAVE, &act);
    CHECK(st == MUI_AA_TIP_IDLE, "leave does not reset");

    CHECK(tip_file("mui_aatip.c", src, sizeof src), "mui_aatip.c is missing");
    for (k = 0; k < sizeof tip_need / sizeof tip_need[0]; k++) {
        CHECK(strstr(src, tip_need[k]) != NULL, "mui_aatip.c lacks %s", tip_need[k]);
    }
    for (k = 0; k < sizeof tip_ban / sizeof tip_ban[0]; k++) {
        CHECK(strstr(src, tip_ban[k]) == NULL, "mui_aatip.c uses %s", tip_ban[k]);
    }
    for (j = 0; j < sizeof classes / sizeof classes[0]; j++) {
        CHECK(tip_file(classes[j], src, sizeof src), "%s is missing", classes[j]);
        for (k = 0; k < sizeof need / sizeof need[0]; k++) {
            CHECK(strstr(src, need[k]) != NULL, "%s does not call %s", classes[j], need[k]);
        }
    }
    CHECK(tip_file("mui_aa_draw.c", src, sizeof src), "mui_aa_draw.c is missing");
    fn = strstr(src, "uint32_t aa_retheme_all(void) {");
    end = fn != NULL ? strstr(fn, "\n}\n") : NULL;
    CHECK(end != NULL && strstr(fn, "aatip_drop(NULL)") != NULL && strstr(fn, "aatip_drop(NULL)") < end, "aa_retheme_all does not drop an open tip");
    CHECK(tip_file("mui_aabtn_class.c", src, sizeof src) && strstr(src, "MUIA_ShortHelp") != NULL, "AABtn must still take MUIA_ShortHelp");
    CHECK(tip_file("mui_aalist_class.c", src, sizeof src) && strstr(src, "MUIA_ShortHelp") == NULL, "AAList passes MUIA_ShortHelp to MUI's own bubble");
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
    /* I667b: the AAButton class stretches itself (MaxWidth MUI_MAXMAX) so the spacer children are gone. */
    CHECK(strstr(src, "mui_make_button_icon(") != NULL && strstr(src, "icon_only, int role") != NULL, "dense rows are not icon-only in mui_ui.c");
}

/* I667b wired: every button the builder makes goes through the one AAButton constructor. The Amiga half cannot run on the host,
 * so the proof is the structure that produces the count: the five call-site families (transport, extras, doors, toolbar,
 * crumbs) reach mui_make_button_i through the wrappers, mui_make_button_i makes the object with mui_aabtn_new only (no stock
 * Group button, no Bodychunk), and the family sizes add up to the 37 buttons the design lists (36 + New Playlist, I667b-2). The rig reads mui_aabtn_made(). */
static const char* fn_body(const char* src, const char* head, char* out, size_t cap) {
    const char* a = strstr(src, head);
    const char* e;
    size_t n;
    if (a == NULL) {
        return NULL;
    }
    e = strstr(a, "\n}\n");
    n = e != NULL ? (size_t)(e - a) : strlen(a);
    n = n < cap - 1 ? n : cap - 1;
    memcpy(out, a, n);
    out[n] = 0;
    return out;
}

static void test_aabtn_wired(const char* src) {
    char body[4000];
    const char* wrappers[] = {"static Object* mui_make_button_w(", "static Object* mui_make_button_icon(", "static Object* mui_make_button_left("};
    unsigned crumbs = 0, total, k;
    const char* c = strstr(src, "#define MUI_CRUMB_MAX ");
    if (c != NULL) {
        crumbs = (unsigned)strtoul(c + strlen("#define MUI_CRUMB_MAX "), NULL, 10);
    }
    CHECK(fn_body(src, "static Object* mui_make_button_ex(", body, sizeof body) != NULL && strstr(body, "mui_aabtn_new(&sp)") != NULL &&
              strstr(body, "MUIC_Group") == NULL && strstr(body, "MUIC_Text") == NULL && strstr(body, "MUIC_Bodychunk") == NULL,
          "mui_make_button_ex does not build through the AAButton class alone");
    for (k = 0; k < 3; k++) {
        CHECK(fn_body(src, wrappers[k], body, sizeof body) != NULL && strstr(body, "mui_make_button_") != NULL, "wrapper %s skips mui_make_button_i", wrappers[k]);
    }
    CHECK(strstr(src, "MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_HorizSpacing, MUI_BTN_ICON_GAP") == NULL, "mui_ui.c still builds a stock group button");
    CHECK(strstr(src, "mui_aabtn_new(") != NULL && strstr(src, "mui_aabtn_set_caption(s->btn[i], label)") != NULL, "transport labels do not reach the class");
    total = MUI_BTN_COUNT + MUI_EXTRA_COUNT + SEL_UIM_DOOR_COUNT + MUI_TOOL_BTNS + crumbs;
    CHECK(crumbs == 6u && total == 37u, "button families add up to %u (crumbs %u), design lists 37", total, crumbs);
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
            CHECK(a != NULL && mui_icon_body(mui_icon_id(a), 18) != NULL, "transport button %u state %u has no icon", (unsigned)b, (unsigned)st);
            want++;
        }
    }
    for (i = 0; mui_extra_action(i) != NULL; i++, want++) {
        CHECK(mui_icon_body(mui_icon_id(mui_extra_action(i)), 18) != NULL, "extra %s has no icon", mui_extra_action(i));
    }
    for (i = 0; i < SEL_UIM_DOOR_COUNT; i++, want++) {
        CHECK(mui_icon_body(mui_icon_id(sel_uim_door_at(i)->page), 18) != NULL, "door %s has no icon", sel_uim_door_at(i)->page);
    }
    for (i = 0; i < MUI_TOOL_BTNS; i++, want++) {
        CHECK(mui_icon_body(mui_icon_id(mui_tool_labels[i]), 18) != NULL, "tool %s has no icon", mui_tool_labels[i]);
    }
    CHECK(mui_icon_body(mui_icon_id("crumb"), 18) != NULL, "crumb has no icon");
    want++;
    CHECK(mui_icon_resolved() - before == want, "icon lookups %u want %u", (unsigned)(mui_icon_resolved() - before), (unsigned)want);
    CHECK(mui_icon_id("no such button") < 0 && mui_icon_body(-1, 18) == NULL, "unknown key has no icon");
    {
        const uint8_t* body = mui_icon_body(mui_icon_id("play"), 18);
        size_t k, bytes = (size_t)18 * (((18 + 15) / 16) * 2) * 2, ink = 0;
        CHECK(mui_icon_body(0, 17) == NULL && mui_icon_body(0, 24) != NULL, "bodies exist for the tier sizes only");
        for (k = 0; k < bytes; k++) {
            ink += body[k] != 0;
        }
        CHECK(ink > 8, "play body is empty");
    }
    txt_theme_default(&th);
    mui_icon_palette(th.token, 1, pal);
    CHECK(pal[0] == (th.token[SEL_GUI_TOKEN_SURFACE_RAISED] >> 16 & 0xFFu) * 0x01010101u && pal[9] == (th.token[SEL_GUI_TOKEN_TEXT] >> 16 & 0xFFu) * 0x01010101u,
          "palette runs from the button face to the text colour");
    CHECK(MUI_BTN_PAD_X >= 8 && MUI_BTN_PAD_Y >= 4 && mui_model_icon_px() >= 18, "buttons are bigger than the 1 px text-only ones");
    f = fopen(MUI_UI_SOURCE, "rb");
    CHECK(f != NULL, "open %s", MUI_UI_SOURCE);
    if (f == NULL) {
        return;
    }
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "MUIC_Bodychunk") == NULL && strstr(src, "mui_aabtn_new(&sp)") != NULL && strstr(src, "sp.icon = mui_icon_id(icon_key)") != NULL,
          "mui_ui.c builds the icon image outside the AAButton class");
    CHECK(strstr(src, "mui_make_button_left(mui_door_label(i), sel_uim_door_at(i)->page,") != NULL &&
              strstr(src, "mui_make_button_icon(mui_tool_labels[i], mui_tool_labels[i],") != NULL && strstr(src, "mui_make_button_w(s->crumb_lbl[k], \"crumb\",") != NULL &&
              strstr(src, "mui_make_button_i(s->btn_label[i], mui_transport_widest(i), mui_transport_action(") != NULL && strstr(src, "mui_make_button_icon(mui_extra_label(i), mui_extra_action(i),") != NULL,
          "a button call site passes no icon key");
    test_aabtn_wired(src);
}

/* I667b mui-aabtn-tokens: face and caption of every role in every state are theme tokens (design section 3.1), picked by the
 * shared maths: the sentinel rises once per call, so a class that bypassed it would leave the count behind. */
static void test_aabtn_tokens(void) {
    txt_theme th;
    const uint32_t* t;
    uint32_t face, text, calls = mui_aa_state_calls();
    int role, st;
    txt_theme_default(&th);
    t = th.token;
    for (st = MUI_AA_NORMAL; st <= MUI_AA_SELECTED; st++) {
        uint32_t want_face = st == MUI_AA_HOVER ? t[SEL_GUI_TOKEN_SURFACE_HOVER]
                             : (st == MUI_AA_PRESSED || st == MUI_AA_SELECTED) ? t[SEL_GUI_TOKEN_SURFACE_PRESSED]
                                                                               : t[SEL_GUI_TOKEN_SURFACE_RAISED];
        mui_aa_button_rgb(t, MUI_AA_ROLE_NORMAL, st, &face, &text);
        CHECK(face == want_face, "normal role state %d face %06X want %06X", st, (unsigned)face, (unsigned)want_face);
        CHECK(text == (st == MUI_AA_DISABLED ? t[SEL_GUI_TOKEN_TEXT_DISABLED] : t[SEL_GUI_TOKEN_TEXT]), "normal role state %d caption %06X", st, (unsigned)text);
        mui_aa_button_rgb(t, MUI_AA_ROLE_ACCENT, st, &face, &text);
        CHECK(text == (st == MUI_AA_DISABLED ? t[SEL_GUI_TOKEN_TEXT_DISABLED] : t[SEL_GUI_TOKEN_ACCENT_TEXT]), "accent role state %d caption %06X", st, (unsigned)text);
        CHECK(st == MUI_AA_PRESSED || st == MUI_AA_SELECTED ? face != t[SEL_GUI_TOKEN_ACCENT] : face == t[SEL_GUI_TOKEN_ACCENT], "accent role state %d face %06X", st, (unsigned)face);
        mui_aa_button_rgb(t, MUI_AA_ROLE_GHOST, st, &face, &text);
        CHECK(st != MUI_AA_NORMAL || face == t[SEL_GUI_TOKEN_SURFACE], "ghost rests on the panel face %06X", (unsigned)face);
        CHECK(st != MUI_AA_SELECTED || face == t[SEL_GUI_TOKEN_SURFACE_RAISED], "selected ghost is raised, face %06X", (unsigned)face);
    }
    /* The accent pair (ACCENT_TEXT on ACCENT) is a theme choice that may miss the gate; aa_text then falls back to TEXTPEN (design 5). */
    for (role = MUI_AA_ROLE_NORMAL; role <= MUI_AA_ROLE_GHOST; role++) {
        if (role == MUI_AA_ROLE_ACCENT) {
            continue;
        }
        mui_aa_button_rgb(t, role, MUI_AA_NORMAL, &face, &text);
        CHECK(mui_theme_readable(text, face), "role %d caption %06X unreadable on %06X", role, (unsigned)text, (unsigned)face);
    }
    CHECK(mui_aa_state_calls() > calls, "state sentinel did not move");
}

static char* ui_source(void) {
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    return n > 0 ? src : NULL;
}

/* I667b-2 mui-aacheck (design 3.1, AACheck): the four prefs checkboxes are built by the AAButton class with role toggle. The Amiga
 * half cannot run here, so the proof is the structure that produces them: the prefs rows with ui CHECK are counted from the
 * uimodel table (4), the SEL_UIM_PREF_CHECK branch of the settings builder calls mui_make_check (no stock Image/CheckMark left),
 * mui_make_check reaches mui_aabtn_new with MUI_AA_ROLE_TOGGLE, and the toggle colours are the named tokens. */
static void test_aacheck(void) {
    char body[4000];
    const char* src = ui_source();
    txt_theme th;
    uint32_t i, checks = 0, face, text;
    mui_aa_metrics_t m;
    const char* br;
    const char* key = "r->ui == (uint32_t)SEL_UIM_PREF_CHECK) {\n        o = ";
    for (i = 0; i < sel_uim_pref_row_count(); i++) {
        checks += sel_uim_pref_row_at(i)->ui == (uint32_t)SEL_UIM_PREF_CHECK;
    }
    CHECK(checks == 4u, "prefs table has %u checkbox rows, design lists 4", (unsigned)checks);
    CHECK(src != NULL, "read mui_ui.c");
    if (src == NULL) {
        return;
    }
    br = strstr(src, key);
    CHECK(br != NULL && strncmp(br + strlen(key), "mui_make_check(", 15) == 0, "the checkbox branch does not build through mui_make_check");
    CHECK(fn_body(src, "static Object* mui_make_check(", body, sizeof body) != NULL && strstr(body, "MUI_AA_ROLE_TOGGLE") != NULL &&
              strstr(body, "mui_aabtn_new(&sp)") != NULL && strstr(body, "MUIC_Image") == NULL,
          "mui_make_check does not build a toggle AAButton");
    CHECK(strstr(src, "MUII_CheckMark") == NULL, "mui_ui.c still builds a stock check image");
    txt_theme_default(&th);
    mui_aa_button_rgb(th.token, MUI_AA_ROLE_TOGGLE, MUI_AA_NORMAL, &face, &text);
    CHECK(face == th.token[SEL_GUI_TOKEN_SURFACE] && text == th.token[SEL_GUI_TOKEN_ACCENT_TEXT], "unchecked box face %06X", (unsigned)face);
    mui_aa_button_rgb(th.token, MUI_AA_ROLE_TOGGLE, MUI_AA_SELECTED, &face, &text);
    CHECK(face == th.token[SEL_GUI_TOKEN_ACCENT] && text == th.token[SEL_GUI_TOKEN_ACCENT_TEXT], "checked box face %06X ink %06X (desktop d_checkbox: accent_text)", (unsigned)face, (unsigned)text);
    mui_aa_button_rgb(th.token, MUI_AA_ROLE_TOGGLE, MUI_AA_DISABLED, &face, &text);
    CHECK(text == th.token[SEL_GUI_TOKEN_TEXT_DISABLED], "disabled check ink %06X", (unsigned)text);
    CHECK(mui_icon_body(mui_icon_id("check"), 18) != NULL, "check icon missing");
    mui_aa_metrics(13, &m);
    CHECK(m.check_px == 16, "box edge %d, design says 16", m.check_px);
    mui_aa_metrics(0, &m);
    CHECK(m.check_px > 0 && m.check_px <= m.height, "tier S box %d does not fit height %d", m.check_px, m.height);
    {
        /* I667b-3: the paint plan the class executes (class source calls it), checked against the desktop d_checkbox. */
        mui_aa_check_op_t op[MUI_AA_CHECK_OPS];
        uint32_t calls = mui_aa_state_calls();
        int n;
        mui_aa_metrics(13, &m);
        n = mui_aa_check_plan(th.token, &m, MUI_AA_NORMAL, op);
        CHECK(mui_aa_state_calls() > calls, "plan did not run the shared colour path");
        CHECK(n == 3 && op[0].inset == 0 && op[0].radius == 0 && op[0].rgb == th.token[SEL_GUI_TOKEN_SURFACE], "corners not filled with the parent background");
        CHECK(op[1].inset == 0 && op[1].radius == (int)th.token[SEL_GUI_TOKEN_RADIUS_SMALL] && op[1].radius < m.check_px / 2 && op[1].radius > 0,
              "off box radius %d is not the small corner radius (circle would be %d)", op[1].radius, m.check_px / 2);
        CHECK(op[1].rgb == th.token[SEL_GUI_TOKEN_TEXT_MUTED] && op[1].rgb != op[2].rgb, "off outline %06X is not the visible muted pen", (unsigned)op[1].rgb);
        CHECK(op[2].inset == 2 && op[2].rgb == th.token[SEL_GUI_TOKEN_SURFACE], "off face inside a 2 px outline wrong");
        n = mui_aa_check_plan(th.token, &m, MUI_AA_SELECTED, op);
        CHECK(n == 2 && op[1].rgb == th.token[SEL_GUI_TOKEN_ACCENT] && op[1].radius == (int)th.token[SEL_GUI_TOKEN_RADIUS_SMALL], "checked plan wrong");
        n = mui_aa_check_plan(th.token, &m, MUI_AA_DISABLED, op);
        CHECK(n == 3 && op[1].rgb == th.token[SEL_GUI_TOKEN_TEXT_DISABLED], "disabled outline wrong");
        mui_aa_metrics(0, &m);
        n = mui_aa_check_plan(th.token, &m, MUI_AA_NORMAL, op);
        CHECK(n == 3 && op[1].radius == (int)th.token[SEL_GUI_TOKEN_RADIUS_SMALL] / 2 && op[2].inset == 1, "tier S plan wrong");
    }
}

/* I667b-2 mui-newplaylist (closes I661): New Playlist is the 12th toolbar button. Label, icon, the id table and the fire path
 * are checked from the structures that produce them: mui_tool_labels, g_tool_which in mui_ui.c (one entry per label, P4_PNEW at
 * the New Playlist slot), and the P4_PNEW case fires mui_viewbtn_fire(1, ...) which sends PLAYLIST_CREATE. */
static void test_newplaylist(void) {
    const char* src = ui_source();
    uint32_t i, at = MUI_TOOL_BTNS, fires = mui_view_fires();
    const char* w;
    const char* p;
    char tbl[600];
    unsigned n = 0, k;
    CHECK(MUI_TOOL_BTNS == 12u, "toolbar has %u buttons, want 12", (unsigned)MUI_TOOL_BTNS);
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if (strcmp(mui_tool_labels[i], "New Playlist") == 0) {
            at = i;
        }
    }
    CHECK(at < MUI_TOOL_BTNS, "mui_tool_labels has no New Playlist");
    CHECK(mui_icon_body(mui_icon_id("New Playlist"), 18) != NULL, "New Playlist has no icon");
    CHECK(src != NULL, "read mui_ui.c");
    if (src == NULL) {
        return;
    }
    w = strstr(src, "g_tool_which[MUI_TOOL_BTNS] = {");
    CHECK(w != NULL, "g_tool_which missing");
    if (w != NULL) {
        w = strchr(w, '{') + 1;
        k = (unsigned)(strchr(w, '}') - w);
        snprintf(tbl, sizeof tbl, "%.*s", (int)k, w);
        for (p = tbl; *p != 0; p++) {
            n += *p == ',';
        }
        CHECK(n + 1 == MUI_TOOL_BTNS, "g_tool_which has %u ids for %u buttons", n + 1, (unsigned)MUI_TOOL_BTNS);
        for (p = tbl, i = 0; i < at && p != NULL; i++) {
            p = strchr(p, ',');
            p = p != NULL ? p + 1 : NULL;
        }
        CHECK(p != NULL && strncmp(p + strspn(p, " \n"), "P4_PNEW", 7) == 0, "id slot %u is not P4_PNEW", (unsigned)at);
    }
    CHECK(strstr(src, "case P4_PNEW:") != NULL && strstr(src, "mui_viewbtn_fire(1,") != NULL, "P4_PNEW does not fire mui_viewbtn_fire(1)");
    CHECK(strstr(src, "\"pnew\"") != NULL, "p4_name has no pnew");
    g_sent = 0;
    CHECK(mui_viewbtn_fire(1, "", 0, fake_send, NULL) == 1 && g_sent == 1 && strcmp(g_sent_line, "PLAYLIST_CREATE \"New Playlist\" 0") == 0, "fire sent '%s'", g_sent_line);
    CHECK(mui_view_fires() == fires + 1u, "fire counter %u want %u", (unsigned)mui_view_fires(), (unsigned)(fires + 1u));
}

/* I670 mui-libfilter (design 2026-10-29 section 4): the sidebar library filter control. The press goes through mui_libfilter_press,
 * the function mui_control_fire calls for CTL_LIB_PREV and CTL_LIB_NEXT; it steps with the desktop chips' clamp and runs the
 * desktop's filter pass. The sentinel is sel_uim_lfilter_passes (one pass per press), so a press that skipped the shared pass fails. */
static void test_libfilter(void) {
    static const char* const want[SEL_UIM_LFILTERS] = {"All", "Playlists", "Albums", "Artists", "Composers", "Sources", "Local Files"};
    sel_engine_library_item items[4];
    char label[32];
    uint32_t f = 0u, rows = 9u, i, passes = sel_uim_lfilter_passes(), presses = mui_libfilter_presses();
    const char* src = ui_source();
    CHECK(SEL_UIM_LFILTERS == 7u, "filter count %u", (unsigned)SEL_UIM_LFILTERS);
    f = mui_libfilter_press(f, -1, NULL, items, 4, &rows, label, sizeof label);
    CHECK(f == 0u && strcmp(label, "All") == 0 && rows == 0u, "previous at All stays: filter %u label '%s'", (unsigned)f, label);
    for (i = 1u; i < SEL_UIM_LFILTERS; i++) {
        f = mui_libfilter_press(f, 1, NULL, items, 4, &rows, label, sizeof label);
        CHECK(f == i && strcmp(label, want[i]) == 0, "next press %u gives filter %u label '%s'", (unsigned)i, (unsigned)f, label);
    }
    f = mui_libfilter_press(f, 1, NULL, items, 4, &rows, label, sizeof label);
    CHECK(f == SEL_UIM_LFILTERS - 1u && strcmp(label, "Local Files") == 0, "next at the last filter stays: %u '%s'", (unsigned)f, label);
    f = mui_libfilter_press(f, -1, NULL, items, 4, &rows, label, sizeof label); /* I831: the left button is the x with a filter in force */
    CHECK(f == 0u && strcmp(label, "All") == 0, "the left button with a filter in force clears to All: %u '%s'", (unsigned)f, label);
    CHECK(mui_libfilter_presses() == presses + SEL_UIM_LFILTERS + 2u, "press counter %u", (unsigned)(mui_libfilter_presses() - presses));
    CHECK(sel_uim_lfilter_passes() == passes + SEL_UIM_LFILTERS + 2u, "the shared filter pass ran %u times, want %u",
          (unsigned)(sel_uim_lfilter_passes() - passes), (unsigned)(SEL_UIM_LFILTERS + 2u));
    CHECK(sel_uim_lfilter_mask(0) == SEL_LIB_ITEM_ALL && sel_uim_lfilter_mask(4) == SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER), "masks");
    /* I669: one row list. The S6 rows are the model rows of the filter in force, so rows drawn == filtered rows, and a chip fit decision
     * shared with the desktop picks chips or the single label. */
    {
        sel_engine_library_item it[4];
        mui_sidebar_in in;
        mui_sidebar_model m;
        uint32_t k, sbuilds = mui_sidebar_builds(), nfilt = 0u;
        memset(it, 0, sizeof it);
        memset(&in, 0, sizeof in);
        for (k = 0; k < 4u; k++) {
            it[k].kind = k < 2u ? (uint32_t)SEL_LIB_ITEM_PLAYLIST : (uint32_t)SEL_LIB_ITEM_ALBUM;
            snprintf(it[k].name, sizeof it[k].name, "row %u", (unsigned)k);
        }
        in.items = it;
        in.item_n = 4u;
        in.filter = "";
        in.chip = 1u; /* Playlists */
        in.chips_single = 1;
        mui_sidebar_build(&in, &m);
        for (k = 0; k < 4u; k++) {
            nfilt += (sel_uim_lfilter_mask(1u) & SEL_LIB_ITEM_BIT(it[k].kind)) != 0u;
        }
        CHECK(m.row_n == nfilt && nfilt == 2u, "rows drawn %u, filtered model rows %u: the sidebar has a second list", (unsigned)m.row_n, (unsigned)nfilt);
        CHECK(m.chip_left_clear && strcmp(m.chip_left_name, "Clear Filter") == 0, "label mode with Playlists in force: left button '%s'", m.chip_left_name); /* I831 */
        CHECK(m.chip_single && strcmp(m.chip_label, "Playlists") == 0, "narrow sidebar: S5 is the label naming the filter, got '%s'", m.chip_label);
        for (k = 0; k < SEL_UIM_SIDE_CHIPS; k++) {
            CHECK(!m.chip_shown[k], "chip %u shows beside the single label", (unsigned)k);
        }
        in.chip = 0u; /* I831: All in force, label mode: Previous Filter */
        mui_sidebar_build(&in, &m);
        CHECK(!m.chip_left_clear && strcmp(m.chip_left_name, "Previous Filter") == 0, "label mode with All in force: left button '%s'", m.chip_left_name);
        in.chips_single = 0;
        mui_sidebar_build(&in, &m);
        CHECK(!m.chip_single && m.chip_shown[0] && m.chip_shown[5] && m.row_n == 4u, "wide sidebar: the six chips show over all %u rows", (unsigned)m.row_n);
        CHECK(mui_sidebar_builds() == sbuilds + 3u, "the sidebar model did not run");
        CHECK(sel_uim_lfilter_chips_single(1, 10, 1000) == 1 && sel_uim_lfilter_chips_single(0, 10, 1000) == 0 && sel_uim_lfilter_chips_single(0, 1001, 1000) == 1,
              "shared chips-or-label decision");
        {
            const int32_t vw[4] = {100, 60, 80, 90}; /* I918: 330 wide plus 3 gaps of 4 */
            CHECK(sel_uim_viz_tabs_fit(vw, 4u, 4, 342) == 1 && sel_uim_viz_tabs_fit(vw, 4u, 4, 341) == 0 && sel_uim_viz_tabs_fit(vw, 1u, 4, 100) == 1 &&
                      sel_uim_viz_tabs_fit(vw, 0u, 4, 100) == 0,
                  "I918 viz tabs fit: exact fit tabs, one pixel short chooser, one label, none");
        }
        CHECK(mui_sidebar_chips_single(0, 100u) == 1 && mui_sidebar_chips_single(0, 2000u) == 0 && mui_sidebar_chips_single(1, 2000u) == 1, "MUI width decision");
    }
    CHECK(src != NULL, "read mui_ui.c");
    if (src != NULL) {
        const char* g = strstr(src, "static Object* mui_sidebar_dyn(const mui_sidebar_model* m) {");
        const char* gs = g != NULL ? strstr(g, "static Object* mui_sidebar_group(") : NULL;
        const char* end = gs != NULL ? strstr(gs, "\n}\n") : NULL;
        char body[12000];
        snprintf(body, sizeof body, "%.*s", g != NULL && end != NULL ? (int)(end - g < 11990 ? end - g : 11990) : 0, g != NULL ? g : "");
        CHECK(strstr(src, "id == CTL_LIB_PREV || id == CTL_LIB_NEXT") != NULL && strstr(src, "mui_libfilter_press(s->lib_filter") != NULL,
              "mui_control_fire does not reach mui_libfilter_press");
        CHECK(strstr(src, "m->chip_left_name") != NULL && strstr(src, "m->chip_left_clear") != NULL && strstr(src, "\"Next Filter\"") != NULL,
              "the sidebar left button does not come from the shared model");
        CHECK(strstr(src, "lib_list") == NULL && strstr(src, "lib_ptr") == NULL && strstr(body, "sel_uim_lfilter_pass(") != NULL,
              "I670's separate list is back, or the sidebar build skips the shared pass");
    }
}

/* Regression (I684a): Selecta_AACycle's OM_SET redrew when _win(obj) != NULL. Before the first MUIM_Setup the render info
 * is NULL, so _win(obj) reads address 0; on rig 2 that held garbage, the redraw ran on an object with no window and the
 * Amiga crashed at start (set AAC_Active on the visualizer cycle in mui_detail_group). Every class must ask aa_live(),
 * which checks the render info first; no class file may test _win(obj) against NULL on its own. */
static void test_live_guard(void) {
    static const char* const files[] = {"mui_aacycle_class.c", "mui_aafield_class.c", "mui_aabtn_class.c", "mui_aascroller_class.c",
                                        "mui_aaslider_class.c", "mui_aalist_class.c", "mui_listview_class.c", "mui_aatext_class.c",
                                        "mui_aapanel_class.c", "mui_vizarea_class.c", "mui_canvas_class.c"};
    static char src[200000];
    char path[1024];
    size_t i, n;
    FILE* f;
    for (i = 0; i < sizeof files / sizeof files[0]; i++) {
        snprintf(path, sizeof path, "%s/%s", MUI_DIR, files[i]);
        f = fopen(path, "rb");
        CHECK(f != NULL, "open %s", path);
        if (f == NULL) {
            continue;
        }
        n = fread(src, 1, sizeof src - 1, f);
        fclose(f);
        src[n] = 0;
        CHECK(strstr(src, "_win(obj) != NULL") == NULL && strstr(src, "_win(obj) == NULL") == NULL,
              "%s tests _win(obj) against NULL without the render info guard (use aa_live)", files[i]);
    }
    snprintf(path, sizeof path, "%s/mui_aa_draw.c", MUI_DIR);
    f = fopen(path, "rb");
    CHECK(f != NULL, "open %s", path);
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(strstr(src, "muiRenderInfo(obj) != NULL && _win(obj) != NULL") != NULL, "aa_live does not check the render info before _win");
}

/* I713: the rig's wasabid lists AAList rows as items by calling AAM_GetRow (the class's documented read-back, mui_aalist_class.h).
 * The class is Amiga-only, so the contract is checked in the sources: the header declares the method and its message, the
 * dispatcher answers it, and the handler gives the row text, the selection and the on-screen box wasabid clicks. */
static void test_aalist_getrow(void) {
    static char src[200000];
    char path[1024];
    size_t n;
    FILE* f;
    snprintf(path, sizeof path, "%s/mui_aalist_class.h", MUI_DIR);
    f = fopen(path, "rb");
    CHECK(f != NULL, "open %s", path);
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(strstr(src, "#define AAM_GetRow") != NULL && strstr(src, "mui_aalist_getrow;") != NULL, "mui_aalist_class.h declares no AAM_GetRow message");
    snprintf(path, sizeof path, "%s/mui_aalist_class.c", MUI_DIR);
    f = fopen(path, "rb");
    CHECK(f != NULL, "open %s", path);
    n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(strstr(src, "case AAM_GetRow:") != NULL, "the AAList dispatcher does not answer AAM_GetRow");
    CHECK(strstr(src, "g->selected = ") != NULL && strstr(src, "g->y = _mtop(obj) + d->m.pad_y + aalist_title_h(d)") != NULL,
          "AAM_GetRow does not give the selection and the row box drawn by aalist_draw");
}

/* Regression (I684a, the crash itself): mui_detail_group built the visualizer cycle and then called set(AAC_Active) before
 * the cycle was in the application tree. The set fired the MUIV_Notify_Application notification with no global info, MUI read
 * the application through a NULL pointer, and rig 2 crashed at every start whose saved visualizer was not the first. The
 * settings cycles had the same shape. The initial entry must go in at OM_NEW (mui_make_cycle's active argument): mui_ui.c may
 * not set AAC_Active at all; only the user's pick changes it. */
/* I740: the transport clock/remaining/length texts and the four status-row labels are plain AAText: no MUI frame, no field face or
 * outline (it made them look like text inputs; the Mac footer draws muted text on the window). Source contract; red when a
 * face/framed path comes back into the constructor or the class. */
static void test_status_face(void) {
    static char cls_buf[100000];
    char path[512];
    char *src = ui_source(), *cls = NULL, *ctor, *grp, *end;
    FILE* cf;
    snprintf(path, sizeof path, "%s/mui_aatext_class.c", MUI_DIR);
    cf = fopen(path, "rb");
    if (cf != NULL) {
        size_t n = fread(cls_buf, 1, sizeof cls_buf - 1, cf);
        fclose(cf);
        cls_buf[n] = 0;
        cls = cls_buf;
    }
    CHECK(src != NULL && cls != NULL, "cannot read mui_ui.c or mui_aatext_class.c");
    if (src == NULL || cls == NULL) {
        return;
    }
    ctor = strstr(src, "static Object* mui_aatext_sized(");
    CHECK(ctor != NULL && strstr(ctor, "sp.frame = (ULONG)MUIV_Frame_None") != NULL && strstr(ctor, "sp.background = 0UL") != NULL, "mui_aatext_sized does not build frameless, backgroundless texts");
    CHECK(strstr(src, "sp.face") == NULL && strstr(src, "framed") == NULL && strstr(src, "MUI_TH_STATUS") == NULL, "mui_ui.c still gives texts a face or framed look");
    CHECK(strstr(cls, "mui_aa_field_plan(") == NULL && strstr(cls, "aa_round_fill(") == NULL && strstr(cls, "MUIA_InnerLeft") == NULL && strstr(cls, "d->face") == NULL,
          "AAText still draws a field face or inner paddings");
    grp = strstr(src, "static Object* mui_player_group(");
    end = grp != NULL ? strstr(grp, "MUI-P3-4") : NULL;
    CHECK(grp != NULL && end != NULL, "mui_player_group not found");
    if (grp != NULL && end != NULL) {
        const char* attrs[] = {"MUI_ATTR_CLOCK", "MUI_ATTR_REMAINING", "MUI_ATTR_LENGTH", "MUI_ATTR_TITLE", "MUI_ATTR_ARTIST", "MUI_ATTR_FORMAT", "MUI_ATTR_STATUS"};
        size_t i;
        for (i = 0; i < sizeof attrs / sizeof attrs[0]; i++) {
            char key[96];
            const char* at;
            snprintf(key, sizeof key, "mui_make_text(s, %s,", attrs[i]);
            at = strstr(grp, key);
            CHECK(at != NULL && at < end, "player group has no %s text box", attrs[i]);
        }
    }
}

/* I717 (moved by I796): the volume slider is in the right column of the player bar, after the seek row's length box in source order, never in the
 * transport row; it is fixed width. */
static void test_volume_row(void) {
    char *src = ui_source(), *grp, *end, *seek, *len, *vol;
    CHECK(src != NULL, "cannot read mui_ui.c");
    if (src == NULL) {
        return;
    }
    grp = strstr(src, "static Object* mui_player_group(");
    end = grp != NULL ? strstr(grp, "MUI-P3-4") : NULL;
    CHECK(grp != NULL && end != NULL, "mui_player_group not found");
    if (grp == NULL || end == NULL) {
        return;
    }
    seek = strstr(grp, "DoMethod(row2, OM_ADDMEMBER, s->seek)");
    len = strstr(grp, "MUI_ATTR_LENGTH");
    vol = strstr(grp, "DoMethod(row4, OM_ADDMEMBER, s->vol)");
    CHECK(seek != NULL && seek < end, "seek slider is not in the seek row");
    CHECK(vol != NULL && vol < end && len != NULL && vol > len, "volume slider is not in the right column");
    CHECK(strstr(grp, "DoMethod(row1, OM_ADDMEMBER, s->vol)") == NULL, "volume slider is in the button row");
    CHECK(strstr(grp, "CTL_VOLUME, MUI_VOLUME_FIX_W)") != NULL, "volume slider has no fixed width");
}

/* I796 (drift B1-B4): ONE reachability test of the player bar. mui_ui.c builds it from the map layer (Mac transport order, Mac column weights,
 * art frame, Like beside the title), and the icons it builds fire their commands: each transport action and Like go through mui_action_fire
 * (the handler sentinel mui_action_fires() counts them). */
static int bar_sent;
static char bar_line[16][64];
static int bar_send(void* ctx, const char* line) {
    (void)ctx;
    if (bar_sent < 16) {
        snprintf(bar_line[bar_sent], sizeof bar_line[0], "%s", line);
    }
    bar_sent++;
    return 1;
}

static void test_player_bar(void) {
    static const uint32_t mac[MUI_BTN_COUNT] = {MUI_BTN_RANDOM, MUI_BTN_PREV, MUI_BTN_PLAY, MUI_BTN_NEXT, MUI_BTN_REPEAT, MUI_BTN_STOP};
    char *src = ui_source(), *grp, *end, *ord, *tool, *tend;
    sel_uim_frame f;
    uint32_t k, base = mui_action_fires();
    memset(&f, 0, sizeof f);
    for (k = 0; k < MUI_BTN_COUNT; k++) {
        const char* act;
        CHECK(mui_player_transport_at(k) == mac[k], "transport position %u is button %u, want %u", (unsigned)k, (unsigned)mui_player_transport_at(k), (unsigned)mac[k]);
        CHECK(mui_transport_icon_only(mui_player_transport_at(k)), "transport position %u is not an icon", (unsigned)k);
        act = mui_transport_action(mui_player_transport_at(k), &f);
        CHECK(act != NULL && mui_action_fire(act, bar_send, NULL) == 1u, "transport position %u (%s) does not fire", (unsigned)k, act != NULL ? act : "none");
    }
    CHECK(mui_action_fire("like", bar_send, NULL) == 1u, "Like does not fire");
    CHECK(bar_sent == (int)MUI_BTN_COUNT + 1 && mui_action_fires() - base == MUI_BTN_COUNT + 1u, "sentinel %u sent %d", (unsigned)(mui_action_fires() - base), bar_sent);
    CHECK(bar_sent > (int)MUI_BTN_COUNT && strcmp(bar_line[MUI_BTN_COUNT], mui_like_line()) == 0, "Like sends '%s'", bar_line[MUI_BTN_COUNT]);
    CHECK(MUI_PLAYER_W_LEFT + MUI_PLAYER_W_CENTRE + MUI_PLAYER_W_RIGHT == 100 && MUI_PLAYER_W_CENTRE > MUI_PLAYER_W_LEFT, "column weights");
    CHECK(src != NULL, "cannot read mui_ui.c");
    if (src == NULL) {
        return;
    }
    grp = strstr(src, "static Object* mui_player_group(");
    end = grp != NULL ? strstr(grp, "MUI-P3-4") : NULL;
    CHECK(grp != NULL && end != NULL, "mui_player_group not found");
    if (grp == NULL || end == NULL) {
        return;
    }
    ord = strstr(grp, "mui_player_transport_at(k)");
    CHECK(ord != NULL && ord < end, "the transport row is not built in the Mac order of the map layer");
    CHECK(strstr(grp, "mui_aapanel(MUI_AA_PANEL_RAISED, MUI_PLAYER_ART_PX)") != NULL, "no art frame in the player bar");
    CHECK(strstr(grp, "MUIA_HorizWeight, MUI_PLAYER_W_LEFT") != NULL && strstr(grp, "MUIA_HorizWeight, MUI_PLAYER_W_CENTRE") != NULL &&
              strstr(grp, "MUIA_HorizWeight, MUI_PLAYER_W_RIGHT") != NULL,
          "the three columns do not take the map-layer weights");
    CHECK(strstr(grp, "DoMethod(hbar, OM_ADDMEMBER, colL)") != NULL && strstr(grp, "DoMethod(hbar, OM_ADDMEMBER, colC)") != NULL &&
              strstr(grp, "DoMethod(hbar, OM_ADDMEMBER, colR)") != NULL && strstr(grp, "DoMethod(g, OM_ADDMEMBER, hbar)") != NULL,
          "the bar is not one HGroup of three columns");
    CHECK(strstr(grp, "CTL_P4_BASE + P4_LIKE") != NULL && strstr(grp, "DoMethod(row3, OM_ADDMEMBER, like)") != NULL, "Like is not beside the title");
    tool = strstr(src, "static Object* mui_tool_group(");
    tend = tool != NULL ? strstr(tool, "I667i2") : NULL;
    CHECK(tool != NULL && tend != NULL && strstr(tool, "g_tool_which[i] == P4_LIKE") != NULL && strstr(tool, "g_tool_which[i] == P4_LIKE") < tend,
          "the tool rows still build Like");
}

static void test_cycle_initial(void) {
    static char src[1000000];
    const char* line;
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n;
    CHECK(f != NULL, "open %s", MUI_UI_SOURCE);
    if (f == NULL) {
        return;
    }
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    for (line = src; line != NULL && *line != 0;) {
        const char* end = strchr(line, '\n');
        size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
        char buf[512];
        snprintf(buf, sizeof buf, "%.*s", (int)(len < sizeof buf - 1 ? len : sizeof buf - 1), line);
        CHECK(!(strstr(buf, "set(") != NULL && strstr(buf, "AAC_Active") != NULL), "mui_ui.c sets AAC_Active after OM_NEW: %s", buf);
        line = end != NULL ? end + 1 : NULL;
    }
    CHECK(strstr(src, "mui_make_cycle(s->vizset.name_, (LONG)s->vizset.sel,") != NULL, "visualizer cycle does not take the saved pick at OM_NEW");
}

/* I684b: the VizArea and canvas objects must get the real RTG flag of the screen (the foreign-mode test in mui_screen_open),
 * not a literal 1; with 1 a planar AGA screen still ran the pixel step although nothing can draw there. */
static void test_screen_rtg_flag(void) {
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(strstr(src, "g_screen_rtg = foreign;") != NULL, "the flag is not taken from the foreign-mode test");
    CHECK(strstr(src, "mui_vizarea_new(&s->vizset, g_tokens, g_screen_rtg)") != NULL, "VizArea does not get the screen RTG flag");
    CHECK(strstr(src, "mui_canvas_new(&grid, g_screen_rtg)") != NULL, "canvas does not get the screen RTG flag");
    CHECK(strstr(src, "mui_vizarea_new(&s->vizset, g_tokens, 1)") == NULL && strstr(src, "mui_canvas_new(&grid, 1)") == NULL,
          "a hardcoded RTG flag came back");
}

/* I918 step 5: the Visualizer page chooser is the tabs-or-cycle pair sized by sel_uim_viz_tabs_fit, only the shown area steps, the page
 * area may grow tall, and an area without a frame paints the window background instead of keeping stale pixels. The fit rule itself is
 * tested in test_libfilter; this proves the MUI code uses it (MUI cannot run on the host). */
static void test_viz_chooser(void) {
    static char ui[1000000];
    static char area[100000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(ui, 1, sizeof ui - 1, f) : 0;
    const char* step;
    if (f != NULL) {
        fclose(f);
    }
    ui[n] = 0;
    f = fopen(MUI_DIR "/mui_vizarea_class.c", "rb");
    n = f != NULL ? fread(area, 1, sizeof area - 1, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    area[n] = 0;
    CHECK(strstr(ui, "sel_uim_viz_tabs_fit(") != NULL, "the chooser does not use the shared tabs-fit rule");
    CHECK(strstr(ui, "mui_vizarea_set_tall(s->viz_page_area)") != NULL, "the page area is not tall");
    step = strstr(ui, "s->pane.pane == MUI_PANE_VIZ ? s->viz_page_area : s->viz_area");
    CHECK(step != NULL, "the wait turn does not pick the one shown area");
    CHECK(strstr(ui, "mui_viz_chooser_update(s)") != NULL, "the chooser is never refreshed");
    CHECK(strstr(area, "MUIM_DrawBackground") != NULL, "an area with no frame does not paint the background");
    CHECK(strstr(area, "d->tall ? MUI_MAXMAX") != NULL, "the tall flag does not lift the height cap");
}

/* I794: the lines the route handler sends, recorded. */
typedef struct route_log {
    char line[16][160];
    uint32_t n;
} route_log;

static int route_rec(void* ctx, const char* line) {
    route_log* l = (route_log*)ctx;
    if (l->n < 16u) {
        snprintf(l->line[l->n], sizeof l->line[0], "%s", line);
        l->n++;
    }
    return 1;
}

/* I797 (drift doc S1, S2): the sidebar brand and the door icons with the selected state. One reachability test: the model carries the brand,
 * every shown door has an icon, a door press through the route handler moves the lit door (sentinel mui_route_lit_calls), and mui_ui.c builds
 * the brand and lights the door buttons from it. */
static void test_sidebar_icons(void) {
    mui_sidebar_in in;
    mui_sidebar_model m;
    mui_route r;
    route_log l;
    char* src = ui_source();
    uint32_t k, lit0 = mui_route_lit_calls();
    memset(&in, 0, sizeof in);
    memset(&l, 0, sizeof l);
    mui_route_init(&r);
    mui_sidebar_build(&in, &m);
    CHECK(strcmp(m.brand, "Selecta") == 0 && mui_icon_id(m.brand_icon) >= 0, "brand '%s' icon '%s'", m.brand, m.brand_icon);
    in.bitmap = 1;
    mui_sidebar_build(&in, &m);
    CHECK(strcmp(m.brand, "Selecta") == 0 && m.brand_icon[0] == '\0', "the bitmap tier keeps the text and drops the logo");
    in.bitmap = 0;
    mui_sidebar_build(&in, &m);
    CHECK(mui_route_lit_door(&r) == 0, "a fresh window is on Home");
    for (k = 1; k < m.door_n; k++) {
        const char* dl = mui_door_line(m.door[k]);
        CHECK(mui_icon_id(sel_uim_door_at(m.door[k])->page) >= 0, "door %u has no icon", (unsigned)m.door[k]);
        if (dl != NULL) {
            mui_route_door(&r, m.door[k], dl, route_rec, &l);
            CHECK(mui_route_lit_door(&r) == (int)m.door[k], "door %u pressed, lit door %d", (unsigned)m.door[k], mui_route_lit_door(&r));
        }
    }
    mui_route_door(&r, 0u, "VIEW_SOURCE home", route_rec, &l);
    CHECK(mui_route_lit_door(&r) == 0, "Home lights the Home door again");
    mui_route_click(&r, SEL_ROUTE_SHOW_ALL, 0u, 1u, "", "Shelf", "", route_rec, &l);
    CHECK(mui_route_lit_door(&r) == -1, "a shelf page lights no door");
    CHECK(mui_route_lit_calls() - lit0 >= m.door_n, "sentinel: %u", (unsigned)(mui_route_lit_calls() - lit0));
    CHECK(src != NULL, "cannot read mui_ui.c");
    if (src == NULL) {
        return;
    }
    CHECK(strstr(src, "m.brand_icon[0]") != NULL && strstr(src, "mui_side_text(hd, m.brand,") != NULL, "mui_sidebar_group does not build the brand from the model");
    CHECK(strstr(src, "s->door_btn[i] = b") != NULL && strstr(src, "mui_aabtn_set_on(s->door_btn[dk], (int)dk == lit)") != NULL &&
              strstr(src, "mui_route_lit_door(&s->route)") != NULL,
          "the door buttons are not lit from the route history");
}


static int pal_send_n;
static char pal_send_last[64];
static int pal_send(void* ctx, const char* line) {
    (void)ctx;
    pal_send_n++;
    snprintf(pal_send_last, sizeof pal_send_last, "%s", line);
    return 1;
}

/* I798 (drift doc O1, O3, T2, R3): the command palette rows are the Mac's shared table and a pick runs a bare verb or fills the
 * command line; every tooltip text comes from mui_tip (the Mac's sel_uim_tip_* strings and the action tip); Hide / Show Now Playing
 * flips the detail slot of the layout like the Mac's detail_hidden. The wiring checks read mui_ui.c. */
static void test_overlays(void) {
    static mui_palette pal;
    mui_layout_map_t m;
    char fill[64], tip[96];
    sel_uim_frame fr;
    uint32_t i, r, before_tip = mui_tip_calls(), before_np = mui_np_toggles(), main_w, bare = 0, args = 0, all;
    char* src = ui_source();
    memset(&fr, 0, sizeof fr);
    mui_palette_open(&pal);
    all = pal.count;
    CHECK(all > 0u && all == pal.pal.count && pal.opens >= 1u, "palette rows %u, shared %u", (unsigned)all, (unsigned)pal.pal.count);
    CHECK(strcmp(mui_palette_row(&pal, 0), pal.row[0]) == 0 && mui_palette_row(&pal, (long)pal.count)[0] == '\0', "row callback");
    mui_palette_filter(&pal, "pause");
    CHECK(pal.count > 0u && pal.count <= all, "a query narrows %u of %u", (unsigned)pal.count, (unsigned)all);
    mui_palette_filter(&pal, "");
    for (i = 0; i < pal.count && (bare == 0u || args == 0u); i++) {
        const char* verb = pal.pal.items[i].name;
        fill[0] = '\0';
        pal_send_n = 0;
        r = mui_palette_pick(&pal, (long)i, pal_send, NULL, fill, sizeof fill);
        if (verb == NULL) {
            continue;
        }
        if (txt_palette_verb_needs_args(verb)) {
            CHECK(r == MUI_PAL_FILL && pal_send_n == 0 && strncmp(fill, verb, strlen(verb)) == 0 && fill[strlen(verb)] == ' ', "verb '%s' fills '%s' (r %u)", verb, fill, (unsigned)r);
            args++;
        } else {
            CHECK(r == MUI_PAL_RUN && pal_send_n == 1 && strcmp(pal_send_last, verb) == 0, "verb '%s' sent '%s' (r %u)", verb, pal_send_last, (unsigned)r);
            bare++;
        }
    }
    CHECK(bare > 0u && args > 0u && pal.runs >= bare && pal.fills >= args, "sentinels runs %u fills %u", (unsigned)pal.runs, (unsigned)pal.fills);
    CHECK(mui_palette_pick(&pal, -1, pal_send, NULL, fill, sizeof fill) == MUI_PAL_NONE, "a row outside the list picks nothing");

    CHECK(mui_tip(MUI_TIP_LIKE, 0, NULL, 0, tip, sizeof tip) > 0u && strcmp(tip, sel_uim_tip_like(0)) == 0, "like tip '%s'", tip);
    CHECK(mui_tip(MUI_TIP_LIKE, 0, NULL, 1, tip, sizeof tip) > 0u && strcmp(tip, sel_uim_tip_like(1)) == 0, "liked tip '%s'", tip);
    CHECK(mui_tip(MUI_TIP_NP_PANEL, 0, NULL, 1, tip, sizeof tip) > 0u && strcmp(tip, sel_uim_tip_np_panel(1)) == 0, "np tip '%s'", tip);
    CHECK(mui_tip(MUI_TIP_NP_PANEL, 0, NULL, 0, tip, sizeof tip) > 0u && strcmp(tip, sel_uim_tip_np_panel(0)) == 0, "np tip '%s'", tip);
    for (i = 0; i < MUI_BTN_COUNT; i++) {
        CHECK(mui_tip(MUI_TIP_TRANSPORT, i, &fr, 0, tip, sizeof tip) > 0u, "transport button %u has no tip", (unsigned)i);
    }
    CHECK(mui_tip(MUI_TIP_DOOR, 1, NULL, 0, tip, sizeof tip) > 0u, "door 1 has no tip");
    CHECK(mui_tip(MUI_TIP_PALETTE, 0, NULL, 0, tip, sizeof tip) > 0u && mui_tip(MUI_TIP_SEEK, 0, NULL, 0, tip, sizeof tip) > 0u &&
              mui_tip(MUI_TIP_VOLUME, 0, NULL, 0, tip, sizeof tip) > 0u && mui_tip(MUI_TIP_SEARCH, 0, NULL, 0, tip, sizeof tip) > 0u &&
              mui_tip(MUI_TIP_CMD, 0, NULL, 0, tip, sizeof tip) > 0u,
          "a control tip is empty");
    CHECK(mui_tip_calls() - before_tip >= 14u, "sentinel: %u tip calls", (unsigned)(mui_tip_calls() - before_tip));

    memset(&m, 0, sizeof m);
    mui_layout_map(&m, 1400u, 800u, SEL_UIM_TIER_DEFAULT);
    CHECK(mui_np_toggle_shown(&m) && m.show[MUI_SLOT_DETAIL] && !m.detail_hidden, "three columns show the detail slot and the toggle");
    main_w = m.weight_w[MUI_SLOT_MAIN];
    CHECK(mui_np_toggle(&m) == 1u && !m.show[MUI_SLOT_DETAIL] && m.weight_w[MUI_SLOT_MAIN] > main_w, "hidden drops the detail slot and widens main");
    mui_layout_relayout(&m, 1500u, 800u, SEL_UIM_TIER_DEFAULT, NULL);
    CHECK(m.detail_hidden && !m.show[MUI_SLOT_DETAIL] && mui_np_toggle_shown(&m), "a resize keeps the panel hidden and the button shown");
    CHECK(mui_np_toggle(&m) == 0u && m.show[MUI_SLOT_DETAIL], "toggle shows the panel again");
    CHECK(mui_np_toggles() - before_np == 2u, "sentinel: %u flips", (unsigned)(mui_np_toggles() - before_np));
    mui_layout_map(&m, 1000u, 800u, SEL_UIM_TIER_DEFAULT);
    CHECK(!mui_np_toggle_shown(&m), "two columns have no panel to hide");
    mui_layout_map(&m, 1400u, 800u, SEL_UIM_TIER_BITMAP);
    CHECK(!mui_np_toggle_shown(&m), "the bitmap tier has no top bar button");

    CHECK(src != NULL, "cannot read mui_ui.c");
    if (src == NULL) {
        return;
    }
    CHECK(strstr(src, "mui_tip(kind, index, f, flag,") != NULL && strstr(src, "MUIA_ShortHelp") != NULL && strstr(src, "MUI_TIP_DOOR,") != NULL &&
              strstr(src, "MUI_TIP_TRANSPORT,") != NULL && strstr(src, "MUI_TIP_LIKE,") != NULL && strstr(src, "MUI_TIP_SEEK,") != NULL,
          "tooltips are not set from mui_tip");
    CHECK(strstr(src, "CTL_NP_TOGGLE") != NULL && strstr(src, "mui_np_toggle(&s->lay)") != NULL && strstr(src, "mui_layout_map_panel(&s->lay") != NULL &&
              strstr(src, "mui_np_toggle_shown(&s->lay)") != NULL,
          "the Now Playing toggle is not wired");
    CHECK(strstr(src, "CTL_PAL_BTN") != NULL && strstr(src, "mui_palette_open(&s->pal)") != NULL && strstr(src, "mui_palette_filter(&s->pal,") != NULL &&
              strstr(src, "mui_palette_pick(&s->pal,") != NULL && strstr(src, "mui_palette_sync(s)") != NULL,
          "the command palette is not wired");
}

/* I794 behaviour (drift doc T1, P2, P3): each route target enters its page with the lines the desktop UI sends, Back and Forward restore
 * without touching Recents (" 1"), the door keeps its lines as the history entry, and Back and Forward are enabled from the history. */
static void test_route(void) {
    mui_route r;
    route_log l;
    memset(&l, 0, sizeof l);
    mui_route_init(&r);
    CHECK(!mui_route_can(&r, -1) && !mui_route_can(&r, 1), "a fresh history has neither Back nor Forward");
    CHECK(mui_route_click(&r, SEL_ROUTE_SHOW_ALL, 0u, 2u, "", "Made for you", "", route_rec, &l) && mui_route_on_shelf(&r) && l.n == 1u &&
              strcmp(l.line[0], "VIEW_SOURCE home") == 0, "SHOW_ALL opens the Shelf page and reads the Home source: %s", l.n > 0u ? l.line[0] : "(none)");
    CHECK(mui_route_can(&r, -1) && !mui_route_can(&r, 1), "after one click Back is enabled, Forward is not");
    l.n = 0;
    CHECK(mui_route_click(&r, SEL_ROUTE_TRACK_ARTIST, (uint32_t)SEL_LIB_ITEM_ARTIST, 0u, "Jogeir", "Jogeir", "", route_rec, &l) && mui_route_on_collection(&r) &&
              l.n == 1u && strstr(l.line[0], "COLLECTION_OPEN") == l.line[0] && strstr(l.line[0], "\"Jogeir\"") != NULL, "TRACK_ARTIST opens the artist page: %s", l.n > 0u ? l.line[0] : "(none)");
    l.n = 0;
    CHECK(mui_route_click(&r, SEL_ROUTE_TRACK_ALBUM, (uint32_t)SEL_LIB_ITEM_ALBUM, 0u, "Nexus", "Nexus", "Jogeir", route_rec, &l) && mui_route_on_collection(&r) &&
              l.n == 1u && strstr(l.line[0], "\"Nexus\"") != NULL, "TRACK_ALBUM opens the album page");
    l.n = 0;
    CHECK(mui_route_click(&r, SEL_ROUTE_META_ARTIST, (uint32_t)SEL_LIB_ITEM_ARTIST, 0u, "Jogeir", "Jogeir", "", route_rec, &l) && l.n == 1u &&
              strstr(l.line[0], "\"Jogeir\"") != NULL, "META_ARTIST opens the artist page");
    l.n = 0;
    CHECK(mui_route_click(&r, SEL_ROUTE_ALBUM_CARD, (uint32_t)SEL_LIB_ITEM_ALBUM, 0u, "Other", "Other", "", route_rec, &l) && l.n == 1u &&
              strstr(l.line[0], "\"Other\"") != NULL, "ALBUM_CARD opens the album page");
    l.n = 0;
    CHECK(mui_route_walk(&r, -1, route_rec, &l) && l.n == 1u && strstr(l.line[0], "\"Jogeir\"") != NULL && l.line[0][strlen(l.line[0]) - 1u] == '1' &&
              mui_route_can(&r, 1), "Back re-enters the previous collection with the restore flag and enables Forward: %s", l.n > 0u ? l.line[0] : "(none)");
    l.n = 0;
    CHECK(mui_route_walk(&r, 1, route_rec, &l) && l.n == 1u && strstr(l.line[0], "\"Other\"") != NULL && !mui_route_can(&r, 1), "Forward returns to the newest page");
    l.n = 0;
    CHECK(mui_route_door(&r, 5u, "FORMATS_SHOW;COMPOSERS_SHOW;SOURCES", route_rec, &l) && l.n == 3u && strcmp(l.line[1], "COMPOSERS_SHOW") == 0 &&
              !mui_route_on_collection(&r), "a door page sends its lines one by one");
    l.n = 0;
    CHECK(mui_route_walk(&r, -1, route_rec, &l) && mui_route_on_collection(&r), "Back from a door page returns to the page before it");
    mui_route_init(&r);
    CHECK(!mui_route_walk(&r, -1, route_rec, &l) && r.entered == 0u, "Back with no history does nothing");
}

/* I794 reachability: the one test that drives the MUI's real click path. The control handler of mui_ui.c reaches the handler through the
 * wiring grepped below; the handler is then driven the way the handler drives it (counter and sentinel), and Back returns to the previous
 * page. Removing the wiring from mui_ui.c turns the source checks red. */
static void test_route_reach(void) {
    mui_route r;
    route_log l;
    char* src = ui_source();
    memset(&l, 0, sizeof l);
    mui_route_init(&r);
    CHECK(src != NULL, "mui_ui.c source readable");
    CHECK(strstr(src, "#include \"map/mui_route.h\"") != NULL && strstr(src, "mui_route_walk(&s->route, id == CTL_NAV_BACK ? -1 : 1") != NULL,
          "Back and Forward are wired to the history walk");
    CHECK(strstr(src, "\"nav_back\", NULL, CTL_NAV_BACK") != NULL && strstr(src, "mui_route_can(&s->route, -1)") != NULL, "Back button is built and enabled from the history");
    CHECK(strstr(src, "mui_route_door(&s->route") != NULL && strstr(src, "mui_route_click(&s->route, SEL_ROUTE_SHOW_ALL") != NULL &&
              strstr(src, "mui_route_click(&s->route, SEL_ROUTE_HOME_TILE") != NULL && strstr(src, "mui_route_click(&s->route, SEL_ROUTE_META_ARTIST") != NULL,
          "doors, Show all, library tiles and the Artist link go through the route handler");
    CHECK(strstr(src, "mui_col_page_group(s)") != NULL && strstr(src, "mui_route_sync(s, st)") != NULL, "the collection page is built and synced from the history");
    CHECK(mui_route_click(&r, SEL_ROUTE_HOME_TILE, (uint32_t)SEL_LIB_ITEM_ALBUM, 0u, "Nexus", "Nexus", "", route_rec, &l) && r.rh.calls == 1u && r.entered == 1u,
          "the handler ran: route calls %u, pages entered %u", (unsigned)r.rh.calls, (unsigned)r.entered);
    CHECK(mui_route_on_collection(&r) && strstr(l.line[0], "COLLECTION_OPEN") == l.line[0], "the album page was entered through the engine line");
    l.n = 0;
    CHECK(mui_route_walk(&r, -1, route_rec, &l) && !mui_route_on_collection(&r) && r.entered == 2u && strcmp(l.line[0], "VIEW_SOURCE home") == 0 &&
              !mui_route_can(&r, -1), "Back returned to Home and re-sent its source line");
}

/* I795 reachability (drift doc P1, P4, P5): the one test for Home shelves and quick tiles, Browse tiles and Search facet chips. The wiring of
 * mui_ui.c is grepped (removing it turns this red); the page models are driven on a synthetic engine view, and the shelf, tile and facet
 * clicks run through the route handler with the call counter and sentinel. */
static void test_tiles_reach(void) {
    static sel_view_row rows[12];
    static sel_state st;
    sel_view_state v;
    mui_home_page hp;
    mui_shelf sh;
    mui_btile tiles[MUI_TILE_MAX];
    char line[48];
    mui_route r;
    route_log l;
    uint32_t n;
    char* src = ui_source();
    memset(&l, 0, sizeof l);
    memset(rows, 0, sizeof rows);
    memset(&v, 0, sizeof v);
    memset(&st, 0, sizeof st);
    mui_route_init(&r);
    CHECK(src != NULL, "mui_ui.c source readable");
    CHECK(strstr(src, "mui_home_split(") != NULL && strstr(src, "mui_shelf_find(") != NULL && strstr(src, "mui_home_fill(") != NULL, "Home quick row and shelves are built from the view");
    CHECK(strstr(src, "mui_browse_tiles(") != NULL && strstr(src, "mui_tile_fire(s, (uint32_t)(id - CTL_TILE_BASE))") != NULL, "Browse tiles are built and fire");
    CHECK(strstr(src, "mui_search_facet_line(") != NULL && strstr(src, "sel_uim_search_facets") != NULL && strstr(src, "CTL_FACET_BASE") != NULL, "Search facet chips are built and fire");
    CHECK(strstr(src, "mui_route_click(&s->route, SEL_ROUTE_SHOW_ALL, 0u, s->shelf_id[k]") != NULL &&
              strstr(src, "mui_route_click(&s->route, SEL_ROUTE_BROWSE_TILE") != NULL && strstr(src, "mui_home_row_fire(s, view, idx)") != NULL,
          "Show all, composer tiles and quick tiles go through the route handler");
    /* Home: heading quick + 2 tiles, shelf A + 3 rows, shelf B + 1 row */
    rows[0].kind = SEL_ROW_HEADING; rows[0].quick = 1; rows[0].text = "Quick";
    rows[1].kind = SEL_ROW_HOME; rows[2].kind = SEL_ROW_HOME;
    rows[3].kind = SEL_ROW_HEADING; rows[3].text = "Made for you";
    rows[4].kind = SEL_ROW_HOME; rows[5].kind = SEL_ROW_HOME; rows[6].kind = SEL_ROW_HOME;
    rows[7].kind = SEL_ROW_HEADING; rows[7].text = "Recently played";
    rows[8].kind = SEL_ROW_HOME;
    v.rows = rows;
    v.row_n = 9;
    mui_home_split(&v, &hp);
    CHECK(hp.quick_n == 2u && hp.quick_first == 1u && hp.shelf_n == 2u && hp.shelf[0].count == 3u && hp.shelf[0].first == 4u && hp.shelf[1].count == 1u &&
              hp.shelf[1].id == 2u, "home split: quick %u, shelves %u, first shelf %u rows", (unsigned)hp.quick_n, (unsigned)hp.shelf_n, (unsigned)hp.shelf[0].count);
    CHECK(mui_shelf_find(&v, 2u, &sh) == 1u && sh.first == 8u && sh.count == 1u && mui_shelf_find(&v, 3u, &sh) == 0u, "the Shelf page finds only the chosen shelf");
    CHECK(mui_route_click(&r, SEL_ROUTE_SHOW_ALL, 0u, hp.shelf[1].id, "", "Recently played", "", route_rec, &l) && mui_route_on_shelf(&r) && r.rh.calls == 1u &&
              r.entered == 1u, "Show all ran through the route handler: calls %u, entered %u", (unsigned)r.rh.calls, (unsigned)r.entered);
    /* Browse tiles */
    st.formats = "MOD\0" "12\0" "XM\0" "1\0";
    st.composers = "jogeir\0" "Jogeir Liljedahl\0" "7\0";
    n = mui_browse_tiles(&st, tiles, MUI_TILE_MAX);
    CHECK(n == 4u && tiles[0].kind == MUI_TILE_FORMAT && strcmp(tiles[0].arg, "mod") == 0 && tiles[0].count == 12u && tiles[2].kind == MUI_TILE_COMPOSER &&
              strcmp(tiles[2].arg, "jogeir") == 0 && strcmp(tiles[2].label, "Jogeir Liljedahl") == 0 && tiles[3].kind == MUI_TILE_SOURCES,
          "browse tiles: %u tiles (2 formats, 1 composer, Sources)", (unsigned)n);
    memset(&l, 0, sizeof l);
    CHECK(mui_route_click(&r, SEL_ROUTE_BROWSE_TILE, (uint32_t)SEL_LIB_ITEM_COMPOSER, 0u, tiles[2].arg, tiles[2].label, "", route_rec, &l) && mui_route_on_collection(&r) &&
              l.n == 1u && strstr(l.line[0], "COLLECTION_OPEN") == l.line[0] && r.rh.calls == 2u, "a composer tile opens its collection page: %s", l.n > 0u ? l.line[0] : "(none)");
    /* Search facets */
    rows[0].kind = SEL_ROW_HEADING; rows[0].quick = 0; rows[0].text = "Songs (4)";
    rows[5].kind = SEL_ROW_HEADING; rows[5].text = "Albums (2)";
    rows[8].kind = SEL_ROW_HEADING; rows[8].text = "Artists (1)";
    v.window_start = 100;
    CHECK(mui_search_facet_line(&v, 0u, line, sizeof line) == 1u && strcmp(line, "VIEW_CURSOR 0") == 0 && mui_search_facet_line(&v, 2u, line, sizeof line) == 1u &&
              strcmp(line, "VIEW_CURSOR 105") == 0 && mui_search_facet_line(&v, SEL_UIM_SEARCH_FACETS, line, sizeof line) == 0u, "facet chips jump to their group heading: %s", line);
}

/* I949a: one pixel-aspect source. 640x256 PAL non-laced reports tall pixels (22x44 ticks, 512), laced and square modes about 1:1. */
static void test_aa_aspect(void) {
    static char src[400000];
    FILE* f = fopen(MUI_UI_SOURCE, "rb");
    size_t n = f != NULL ? fread(src, 1, sizeof src - 1, f) : 0;
    const char* open_fn;
    uint32_t before = mui_pixel_aspect_calls();
    mui_aa_metrics_t m;
    if (f != NULL) {
        fclose(f);
    }
    src[n] = 0;
    CHECK(mui_pixel_aspect_q8(22, 44) == 512, "640x256 PAL non-laced gives %d, want 512", mui_pixel_aspect_q8(22, 44));
    CHECK(mui_pixel_aspect_q8(22, 22) == 256 && mui_pixel_aspect_q8(44, 44) == 256, "laced or square modes are not 256");
    CHECK(mui_pixel_aspect_q8(44, 22) == 256 && mui_pixel_aspect_q8(1, 9) == 512 && mui_pixel_aspect_q8(0, 0) == 256, "clamp or zero case wrong");
    CHECK(mui_pixel_aspect_calls() == before + 6u, "call sentinel %u", (unsigned)(mui_pixel_aspect_calls() - before));
    mui_aa_metrics(0, &m);
    CHECK(m.aspect_q8 == 256, "metrics aspect_q8 default is not square");
    open_fn = strstr(src, "static void mui_screen_open(");
    CHECK(open_fn != NULL && strstr(open_fn, "g_pixel_aspect_q8 = mui_pixel_aspect_q8((int)di.Resolution.x, (int)di.Resolution.y);") != NULL,
          "mui_screen_open does not read the aspect from DisplayInfo.Resolution");
    CHECK(open_fn != NULL && strstr(open_fn, "DTAG_DISP, mode) > 0) {\n        g_pixel_aspect_q8") != NULL, "the aspect read does not use the opened screen mode");
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
        test_open_no_suggestion();
    } else if (strcmp(argv[1], "seek-row") == 0) {
        test_seek_row();
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
    } else if (strcmp(argv[1], "theme-live") == 0) {
        test_theme_live(argc > 2 ? argv[2] : ".");
    } else if (strcmp(argv[1], "theme-live-repaint") == 0) {
        test_theme_live_repaint(argc > 2 ? argv[2] : ".");
    } else if (strcmp(argv[1], "theme-file") == 0) {
        test_theme_file(argc > 2 ? argv[2] : ".");
    } else if (strcmp(argv[1], "pens") == 0) {
        test_pens();
    } else if (strcmp(argv[1], "style-menu") == 0) {
        test_style_menu();
    } else if (strcmp(argv[1], "style-mask") == 0) {
        test_style_mask();
    } else if (strcmp(argv[1], "list-pending") == 0) {
        test_list_pending();
    } else if (strcmp(argv[1], "list-dbl-row") == 0) {
        test_list_dbl_row();
    } else if (strcmp(argv[1], "list-backends") == 0) {
        test_list_backends();
    } else if (strcmp(argv[1], "topbar") == 0) {
        test_topbar();
    } else if (strcmp(argv[1], "route") == 0) {
        test_route();
    } else if (strcmp(argv[1], "route-reach") == 0) {
        test_route_reach();
    } else if (strcmp(argv[1], "tiles-reach") == 0) {
        test_tiles_reach();
    } else if (strcmp(argv[1], "doors") == 0) {
        test_doors();
    } else if (strcmp(argv[1], "sidebar") == 0) {
        test_sidebar();
    } else if (strcmp(argv[1], "home") == 0) {
        test_home();
    } else if (strcmp(argv[1], "panes") == 0) {
        test_panes();
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
    } else if (strcmp(argv[1], "aacheck") == 0) {
        test_aacheck();
    } else if (strcmp(argv[1], "libfilter") == 0) {
        test_libfilter();
    } else if (strcmp(argv[1], "newplaylist") == 0) {
        test_newplaylist();
    } else if (strcmp(argv[1], "aabtn-tokens") == 0) {
        test_aabtn_tokens();
    } else if (strcmp(argv[1], "icon-buttons") == 0) {
        test_icon_buttons();
    } else if (strcmp(argv[1], "icon-rows") == 0) {
        test_icon_button_rows();
        test_layout_before_open();
    } else if (strcmp(argv[1], "screen-plan") == 0) {
        test_screen_plan();
    } else if (strcmp(argv[1], "window-borderless") == 0) {
        test_window_borderless();
    } else if (strcmp(argv[1], "toolrows") == 0) {
        test_toolrows();
    } else if (strcmp(argv[1], "tracker") == 0) {
        test_tracker();
    } else if (strcmp(argv[1], "live-guard") == 0) {
        test_live_guard();
        test_aalist_getrow();
    } else if (strcmp(argv[1], "status-face") == 0) {
        test_status_face();
    } else if (strcmp(argv[1], "player-bar") == 0) {
        test_player_bar();
    } else if (strcmp(argv[1], "viz-chooser") == 0) {
        test_viz_chooser();
    } else if (strcmp(argv[1], "sidebar-icons") == 0) {
        test_sidebar_icons();
    } else if (strcmp(argv[1], "tooltip-flat") == 0) {
        test_tooltip_flat();
    } else if (strcmp(argv[1], "dialog-windows") == 0) {
        test_dialog_windows();
    } else if (strcmp(argv[1], "overlays") == 0) {
        test_overlays();
    } else if (strcmp(argv[1], "volume-row") == 0) {
        test_volume_row();
    } else if (strcmp(argv[1], "cycle-initial") == 0) {
        test_cycle_initial();
    } else if (strcmp(argv[1], "aa-aspect") == 0) {
        test_aa_aspect();
    } else if (strcmp(argv[1], "screen-rtg-flag") == 0) {
        test_screen_rtg_flag();
    } else {
        return 2;
    }
    if (failures == 0) {
        puts("mui_contract_test: ok");
    }
    return failures != 0;
}
