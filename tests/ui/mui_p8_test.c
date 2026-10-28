/* MUI-P8 parity tests (plan 2026-10-25-amiga-mui.md section P8). Usage: mui_p8_test <name>. Host only; each test fakes the
 * engine with a send function that records the lines. */
#include <selecta/uimodel.h>
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

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    if (strcmp(argv[1], "transport-extras") == 0) {
        test_transport_extras();
    } else if (strcmp(argv[1], "voices-programs") == 0) {
        test_voices_programs();
    } else {
        printf("unknown test %s\n", argv[1]);
        return 2;
    }
    if (failures == 0) {
        printf("PASS %s\n", argv[1]);
    }
    return failures != 0;
}
