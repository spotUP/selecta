/* T37: command palette and help overlay model (plugins/ui/text/palette.c).
 * Fuzzy ranking, every COMMANDS verb exactly once, grouping by the area
 * field of commands.def, current bindings shown. */
#include "keybind.h"
#include "keys.h"
#include "palette.h"

#include <selecta/protocol.h>

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static txt_palette g_p;
static txt_bindings g_b;

static const txt_palette_item* find(const char* name) {
    uint32_t i;
    for (i = 0; i < g_p.count; i++) {
        if (strcmp(g_p.items[i].name, name) == 0) {
            return &g_p.items[i];
        }
    }
    return NULL;
}

int main(void) {
    static const char* const areas[] = {"transport", "queue", "playlists", "view", "protocol"};
    unsigned char seen[SEL_CMD_COUNT];
    uint32_t i, a, last_area = 0, id;
    const txt_palette_item* it;
    txt_bindings_default(&g_b);

    /* every verb exactly once, grouped in area order */
    txt_palette_open(&g_p, &g_b);
    CHECK(g_p.count == SEL_CMD_COUNT);
    memset(seen, 0, sizeof seen);
    for (i = 0; i < g_p.count; i++) {
        CHECK(g_p.items[i].id < SEL_CMD_COUNT);
        if (g_p.items[i].id < SEL_CMD_COUNT) {
            seen[g_p.items[i].id]++;
        }
        for (a = 0; a < 5 && strcmp(areas[a], g_p.items[i].area) != 0; a++) {
        }
        CHECK(a < 5);
        CHECK(a >= last_area); /* never back to an earlier group */
        last_area = a;
    }
    for (id = 0; id < SEL_CMD_COUNT; id++) {
        CHECK(seen[id] == 1);
        CHECK(sel_cmd_area(id) != NULL);
    }

    /* the areas are the comment sections of commands.def */
    CHECK(strcmp(sel_cmd_area(SEL_CMD_PLAY), "transport") == 0);
    CHECK(strcmp(sel_cmd_area(SEL_CMD_QUEUE_SHUFFLE), "queue") == 0);
    CHECK(strcmp(sel_cmd_area(SEL_CMD_VIEW_CURSOR), "view") == 0);
    CHECK(strcmp(sel_cmd_area(SEL_CMD_COMMANDS), "protocol") == 0);
    CHECK(sel_cmd_area(SEL_CMD_COUNT) == NULL);
    CHECK(strcmp(txt_palette_area_title("playlists"), "Playlists and library") == 0);
    CHECK(strcmp(txt_palette_area_title("view"), "View model and shell") == 0);

    /* the current binding is shown */
    it = find("NEXT");
    CHECK(it != NULL && strcmp(it->key, "n") == 0);
    it = find("QUEUE_SHUFFLE");
    CHECK(it != NULL && it->key[0] == '\0');

    /* query qsh: QUEUE_SHUFFLE first */
    txt_palette_set_query(&g_p, "qsh");
    CHECK(g_p.count >= 1);
    CHECK(g_p.count < SEL_CMD_COUNT);
    CHECK(g_p.count > 0 && strcmp(g_p.items[0].name, "QUEUE_SHUFFLE") == 0);

    /* a contiguous match beats a scattered one */
    txt_palette_set_query(&g_p, "next");
    CHECK(g_p.count > 0 && strcmp(g_p.items[0].name, "NEXT") == 0);

    /* no match: empty list, Enter picks nothing */
    txt_palette_set_query(&g_p, "zzzz");
    CHECK(g_p.count == 0);
    CHECK(txt_palette_key(&g_p, 13, 0) == TXT_PAL_EDITING);

    /* keys: type, move, run */
    txt_palette_open(&g_p, &g_b);
    CHECK(txt_palette_key(&g_p, 'q', 0) == TXT_PAL_EDITING);
    CHECK(txt_palette_key(&g_p, 's', 0) == TXT_PAL_EDITING);
    CHECK(txt_palette_key(&g_p, 'h', 0) == TXT_PAL_EDITING);
    CHECK(strcmp(g_p.query, "qsh") == 0);
    CHECK(txt_palette_key(&g_p, 13, 0) == TXT_PAL_RUN);
    CHECK(strcmp(txt_palette_picked(&g_p), "QUEUE_SHUFFLE") == 0);
    txt_palette_open(&g_p, &g_b);
    CHECK(txt_palette_key(&g_p, TXT_KEY_DOWN, 0) == TXT_PAL_EDITING);
    CHECK(g_p.cursor == 1);
    CHECK(txt_palette_key(&g_p, TXT_KEY_UP, 0) == TXT_PAL_EDITING);
    CHECK(g_p.cursor == 0);
    CHECK(txt_palette_key(&g_p, 127, 0) == TXT_PAL_CANCEL); /* empty query */
    CHECK(txt_palette_key(&g_p, 27, 0) == TXT_PAL_CANCEL);

    /* a rebound key shows the new key */
    CHECK(txt_bindings_load(&g_b, "unbind n\nx NEXT\n", NULL) == 0);
    txt_palette_open(&g_p, &g_b);
    it = find("NEXT");
    CHECK(it != NULL && strcmp(it->key, "x") == 0);

    printf(g_fail == 0 ? "palette_test: ok\n" : "palette_test: %d failures\n", g_fail);
    (void)areas;
    return g_fail == 0 ? 0 : 1;
}
