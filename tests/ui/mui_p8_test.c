/* MUI-P8 parity tests (plan 2026-10-25-amiga-mui.md section P8). Usage: mui_p8_test <name>. Host only; each test fakes the
 * engine with a send function that records the lines. */
#include <selecta/uimodel.h>

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

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    if (strcmp(argv[1], "transport-extras") == 0) {
        test_transport_extras();
    } else {
        printf("unknown test %s\n", argv[1]);
        return 2;
    }
    if (failures == 0) {
        printf("PASS %s\n", argv[1]);
    }
    return failures != 0;
}
