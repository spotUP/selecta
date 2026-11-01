/* I970: the 16x8 tall-pixel icon set keeps its detail. tools/gui_iconbake.c grid-fits the non-square set (axis-aligned
 * strokes land on one pixel row or column at full pen, dots survive); the bodies are read back from the baked table
 * (mui_icon_sets, the table mui_icon_body_wh serves the buttons from). Usage: mui_icon_fit_test [dump] */
#include <stdint.h>

#include <selecta/gui/gui_icons.h>

#include <stdio.h>
#include <string.h>

#define W 16
#define H 8

static int g_fail;

#define CHECK(cond, ...)                                                                                              \
    do {                                                                                                              \
        if (!(cond)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                               \
            printf(__VA_ARGS__);                                                                                      \
            printf("\n");                                                                                             \
            g_fail++;                                                                                                 \
        }                                                                                                             \
    } while (0)

/* The baked table itself (plugins/ui/mui/CMakeLists.txt generates it from tools/gui_iconbake.c). */
extern const uint8_t* const* const mui_icon_sets[];
extern const uint32_t mui_icon_size_count;
extern const uint32_t mui_icon_sizes[];
extern const uint32_t mui_icon_heights[];

static const uint8_t* body_16x8(int icon) {
    uint32_t z;
    for (z = 0; z < mui_icon_size_count; z++) {
        if (mui_icon_sizes[z] == W && mui_icon_heights[z] == H) {
            return mui_icon_sets[z][icon];
        }
    }
    return NULL;
}

/* Pen (0..3) at x, y of a Bodychunk body: per row plane 0 then plane 1, 16 bit padded rows. */
static int pen_at(const uint8_t* b, int x, int y) {
    int rowb = ((W + 15) / 16) * 2, p0 = (b[y * rowb * 2 + x / 8] >> (7 - x % 8)) & 1, p1 = (b[y * rowb * 2 + rowb + x / 8] >> (7 - x % 8)) & 1;
    return p0 | (p1 << 1);
}

static void dump(const char* name, const uint8_t* b) {
    int x, y;
    printf("%s\n", name);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            putchar(".123"[pen_at(b, x, y)]);
        }
        putchar('\n');
    }
}

static int row_full(const uint8_t* b, int y, int x0, int x1) {
    int x;
    for (x = x0; x <= x1; x++) {
        if (pen_at(b, x, y) != 3) {
            return 0;
        }
    }
    return 1;
}

static int row_empty(const uint8_t* b, int y, int x0, int x1) {
    int x;
    for (x = x0; x <= x1; x++) {
        if (pen_at(b, x, y) != 0) {
            return 0;
        }
    }
    return 1;
}

static void test_list(const uint8_t* b) {
    int y, bars = 0, bullets = 0;
    for (y = 0; y < H; y++) {
        if (row_full(b, y, 6, 12)) { /* bar body: Lucide x 8..21 is columns 5..13 once fitted; test the middle */
            bars++;
            /* a bar is ONE row at full pen: the rows above and below it carry no dim ghost over the bar */
            CHECK(y == 0 || row_empty(b, y - 1, 6, 12), "list: row %d above a bar is not clear", y - 1);
            CHECK(y == H - 1 || row_empty(b, y + 1, 6, 12), "list: row %d below a bar is not clear", y + 1);
        }
    }
    CHECK(bars == 3, "list: %d full pen bar rows, want 3", bars);
    for (y = 0; y < H; y++) {
        if (pen_at(b, 2, y) == 3) { /* bullet column (Lucide x 3) */
            bullets++;
        }
    }
    CHECK(bullets == 3, "list: %d full pen bullets, want 3 (top, middle, bottom)", bullets);
}

static void test_repeat(const uint8_t* b) {
    int y, runs = 0;
    /* Two arrows, one above the other (Lucide x 17..21 is columns 11..13). Each arrowhead keeps both arm ends and its tip
     * at full pen, and the heads do not merge into a blob: the pixels between the upper head's lower arm end and the
     * lower arrow's upper arm end stay clear. */
    CHECK(pen_at(b, 11, 0) == 3 && pen_at(b, 11, 3) == 3, "repeat: upper arrowhead arm ends are not full pen (%d, %d)", pen_at(b, 11, 0), pen_at(b, 11, 3));
    CHECK(pen_at(b, 13, 2) == 3, "repeat: upper arrow tip is not full pen (%d)", pen_at(b, 13, 2));
    CHECK(pen_at(b, 2, 5) == 3, "repeat: lower arrow tip is not full pen (%d)", pen_at(b, 2, 5));
    CHECK(pen_at(b, 11, 4) == 0 && pen_at(b, 12, 4) == 0, "repeat: the two arrowheads merge (pens %d %d between them)", pen_at(b, 11, 4), pen_at(b, 12, 4));
    for (y = 0; y < H; y++) {
        runs += row_full(b, y, 5, 10);
    }
    CHECK(runs == 2, "repeat: %d full pen horizontal rows, want 2", runs);
}

static void test_stop(const uint8_t* b) {
    int top = -1, bottom = -1, left = -1, right = -1, x, y;
    for (y = 0; y < H; y++) {
        if (row_full(b, y, 4, 11)) {
            if (top < 0) {
                top = y;
            }
            bottom = y;
        }
    }
    for (x = 0; x < W; x++) {
        int all = 1;
        for (y = 3; y <= 4; y++) {
            all = all && pen_at(b, x, y) == 3;
        }
        if (all) {
            if (left < 0) {
                left = x;
            }
            right = x;
        }
    }
    CHECK(top >= 0 && bottom > top, "stop: top/bottom edge rows not found (%d, %d)", top, bottom);
    CHECK(left >= 0 && right > left, "stop: left/right edge columns not found (%d, %d)", left, right);
    /* top and bottom edge are each one row; left and right each one column; no dim pen beside an edge */
    if (top >= 0 && bottom > top && left >= 0 && right > left) {
        CHECK(top > 0 ? pen_at(b, 7, top - 1) == 0 : 1, "stop: dim pen above the top edge");
        CHECK(pen_at(b, 7, top + 1) == 0, "stop: dim pen below the top edge");
        CHECK(pen_at(b, left - 1, 3) == 0 && pen_at(b, left + 1, 3) == 0, "stop: vertical edge is not one column wide (left)");
        CHECK(pen_at(b, right - 1, 3) == 0 && pen_at(b, right + 1, 3) == 0, "stop: vertical edge is not one column wide (right)");
        CHECK(pen_at(b, left, 3) == pen_at(b, 7, top), "stop: vertical edge pen %d vs horizontal edge pen %d", pen_at(b, left, 3), pen_at(b, 7, top));
    }
}

int main(int argc, char** argv) {
    const uint8_t* list = body_16x8(SEL_GUI_ICON_LIST);
    const uint8_t* repeat = body_16x8(SEL_GUI_ICON_REPEAT);
    const uint8_t* stop = body_16x8(SEL_GUI_ICON_SQUARE);
    if (list == NULL || repeat == NULL || stop == NULL) {
        printf("FAIL: no 16x8 set in the baked table\n");
        return 1;
    }
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        dump("list", list);
        dump("repeat", repeat);
        dump("stop", stop);
        return 0;
    }
    test_list(list);
    test_repeat(repeat);
    test_stop(stop);
    printf("%s\n", g_fail == 0 ? "mui_icon_fit_test: ok" : "mui_icon_fit_test: FAILED");
    return g_fail == 0 ? 0 : 1;
}
