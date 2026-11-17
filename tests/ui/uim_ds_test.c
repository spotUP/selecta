/* I1440p: the C64 SID tile and drill helpers of uimodel/browse (design 2026-11-16-i1440-deepsid.md 3.3, 3.4), shared by the desktop and text UIs. */
#include <selecta/uimodel/browse.h>

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

int main(void) {
    static const char* const names[SEL_UIM_DS_TILES] = {"Tags", "Charts", "Collections", "Compos", "Groups", "Players"};
    char label[64], arg[64], out[128];
    uint32_t i;
    for (i = 0; i < SEL_UIM_DS_TILES; i++) {
        char want[64];
        CHECK(sel_uim_ds_tile(i, label, sizeof label, arg, sizeof arg) == 1);
        snprintf(want, sizeof want, "C64 SID %s", names[i]);
        CHECK(strcmp(label, want) == 0);
        CHECK(strncmp(arg, "deepsid\x1f", 8) == 0);
    }
    CHECK(sel_uim_ds_tile(SEL_UIM_DS_TILES, label, sizeof label, arg, sizeof arg) == 0);
    CHECK(sel_uim_ds_child("tags", "demo", out, sizeof out) == 1 && strcmp(out, "tag\x1f" "demo") == 0);
    CHECK(sel_uim_ds_child("compos", "1991", out, sizeof out) == 1 && strcmp(out, "year\x1f" "1991") == 0);
    CHECK(sel_uim_ds_child("year", "X", out, sizeof out) == 1 && strncmp(out, "event\x1f", 6) == 0);
    CHECK(sel_uim_ds_child("collections", "a/b", out, sizeof out) == 1 && strncmp(out, "folder\x1f", 7) == 0);
    CHECK(sel_uim_ds_child("nonsense", "x", out, sizeof out) == 0);
    CHECK(strcmp(sel_uim_ds_label("collections", "MUSICIANS/Hubbard_Rob"), "Hubbard_Rob") == 0);
    CHECK(strcmp(sel_uim_ds_label("tags", "a/b"), "a/b") == 0);
    if (g_fail == 0) {
        printf("uim_ds_test: ok\n");
    }
    return g_fail != 0;
}
