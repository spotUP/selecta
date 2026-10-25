/* Unit test of sel_gui_input_map (I242): the pure translation from raw pointer values to sel_gui_input. */
#include <selecta/gui/gui_input.h>
#include <selecta/uimodel/keys.h>

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c) \
    do { \
        if (!(c)) { \
            fprintf(stderr, "input_map_unit: line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static int map(sel_gui_input_map_state* st, uint32_t kind, int32_t x, int32_t y, uint32_t button, uint32_t clicks,
               int32_t dx, int32_t dy, int32_t scale, sel_gui_input* out) {
    sel_gui_raw r;
    memset(&r, 0, sizeof r);
    r.kind = kind;
    r.x = x;
    r.y = y;
    r.button = button;
    r.clicks = clicks;
    r.dx = dx;
    r.dy = dy;
    r.scale = scale;
    return sel_gui_input_map(st, &r, out);
}

int main(void) {
    sel_gui_input_map_state st;
    sel_gui_input in, scripted;
    memset(&st, 0, sizeof st);

    /* button down waits for its up; the up is a click with the position, button and count */
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_DOWN, 640, 120, 1, 1, 0, 0, 0, &in));
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 640, 120, 1, 1, 0, 0, 0, &in));
    CHECK(in.kind == SEL_GUI_IN_CLICK && in.x == 640 && in.y == 120 && in.button == 0u && in.clicks == 1u);
    /* the same event the script builds: `click 640 120` */
    CHECK(sel_gui_input_parse("click 640 120", &scripted) && scripted.kind == in.kind && scripted.x == in.x &&
          scripted.y == in.y && scripted.button == in.button && scripted.clicks == in.clicks);

    /* second click in a row and the secondary button */
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_DOWN, 5, 6, 1, 2, 0, 0, 0, &in));
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 5, 6, 1, 2, 0, 0, 0, &in) && in.clicks == 2u);
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_DOWN, 7, 8, 3, 1, 0, 0, 0, &in));
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 7, 8, 3, 1, 0, 0, 0, &in) && in.kind == SEL_GUI_IN_CLICK && in.button == 1u);
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 7, 8, 1, 0, 0, 0, 0, &in) && in.clicks == 1u); /* a count of 0 is one click */

    /* press, travel, release is a drag from the press point */
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_DOWN, 10, 20, 1, 1, 0, 0, 0, &in));
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 30, 40, 1, 1, 0, 0, 0, &in));
    CHECK(in.kind == SEL_GUI_IN_DRAG && in.x == 10 && in.y == 20 && in.x2 == 30 && in.y2 == 40);
    /* a jitter inside the slop stays a click */
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_DOWN, 10, 20, 1, 1, 0, 0, 0, &in));
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 12, 21, 1, 1, 0, 0, 0, &in) && in.kind == SEL_GUI_IN_CLICK);

    /* the middle button gives nothing */
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_DOWN, 1, 1, 2, 1, 0, 0, 0, &in));
    CHECK(!map(&st, SEL_GUI_RAW_BUTTON_UP, 1, 1, 2, 1, 0, 0, 0, &in));

    /* motion */
    CHECK(map(&st, SEL_GUI_RAW_MOTION, 33, 44, 0, 0, 0, 0, 0, &in) && in.kind == SEL_GUI_IN_MOVE && in.x == 33 &&
          in.y == 44);

    /* wheel: Q16 notches pass through, a zero wheel is dropped */
    CHECK(map(&st, SEL_GUI_RAW_WHEEL, 0, 0, 0, 0, 0, -3 * 65536, 0, &in) && in.kind == SEL_GUI_IN_WHEEL && in.dx == 0 &&
          in.dy == -3 * 65536);
    CHECK(sel_gui_input_parse("wheel 0 -3", &scripted) && scripted.dy == in.dy && scripted.dx == in.dx);
    CHECK(!map(&st, SEL_GUI_RAW_WHEEL, 0, 0, 0, 0, 0, 0, 0, &in));

    /* pinch: the scale is passed through, 0 or less is dropped */
    CHECK(map(&st, SEL_GUI_RAW_PINCH, 0, 0, 0, 0, 0, 0, 81920, &in) && in.kind == SEL_GUI_IN_PINCH && in.scale == 81920);
    CHECK(sel_gui_input_parse("pinch 1.25", &scripted) && scripted.scale == in.scale);
    CHECK(!map(&st, SEL_GUI_RAW_PINCH, 0, 0, 0, 0, 0, 0, 0, &in));

    /* swipe: right is +1 (Back), left -1 (Forward); the side buttons do the same; a zero swipe is dropped */
    CHECK(map(&st, SEL_GUI_RAW_SWIPE, 0, 0, 0, 0, 5, 0, 0, &in) && in.kind == SEL_GUI_IN_SWIPE && in.x == 1);
    CHECK(map(&st, SEL_GUI_RAW_SWIPE, 0, 0, 0, 0, -5, 0, 0, &in) && in.kind == SEL_GUI_IN_SWIPE && in.x == -1);
    CHECK(sel_gui_input_parse("swipe left", &scripted) && scripted.x == in.x);
    CHECK(!map(&st, SEL_GUI_RAW_SWIPE, 0, 0, 0, 0, 0, 0, 0, &in));
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 0, 0, 4, 1, 0, 0, 0, &in) && in.kind == SEL_GUI_IN_SWIPE && in.x == 1);
    CHECK(map(&st, SEL_GUI_RAW_BUTTON_UP, 0, 0, 5, 1, 0, 0, 0, &in) && in.kind == SEL_GUI_IN_SWIPE && in.x == -1);

    /* an ignored kind */
    CHECK(!map(&st, SEL_GUI_RAW_NONE, 1, 2, 1, 1, 1, 1, 1, &in) && in.kind == SEL_GUI_IN_NONE);
    CHECK(!map(&st, 99u, 1, 2, 1, 1, 1, 1, 1, &in) && in.kind == SEL_GUI_IN_NONE);

    if (fails == 0) {
        printf("input_map_unit: ok\n");
    }
    /* raw key and text (I268): the same sel_gui_input the `key` script line builds */
    {
        sel_gui_raw r;
        memset(&r, 0, sizeof r);
        r.kind = SEL_GUI_RAW_KEY;
        r.code = 'p';
        r.mods = TXT_MOD_CTRL;
        CHECK(sel_gui_input_map(&st, &r, &in) && in.kind == SEL_GUI_IN_KEY);
        CHECK(sel_gui_input_parse("key ctrl+p", &scripted) && scripted.kind == in.kind && scripted.code == in.code &&
              scripted.mods == in.mods);
        r.code = TXT_KEY_DOWN;
        r.mods = TXT_MOD_SHIFT | TXT_MOD_ALT;
        CHECK(sel_gui_input_map(&st, &r, &in));
        CHECK(sel_gui_input_parse("key shift+alt+down", &scripted) && scripted.code == in.code && scripted.mods == in.mods);
        memset(&r, 0, sizeof r);
        r.kind = SEL_GUI_RAW_TEXT;
        r.code = ':';
        r.mods = TXT_MOD_CTRL; /* ignored: typed text has no modifiers */
        CHECK(sel_gui_input_map(&st, &r, &in) && in.kind == SEL_GUI_IN_KEY && in.code == ':' && in.mods == 0u);
        CHECK(sel_gui_input_parse("key :", &scripted) && scripted.code == in.code && scripted.mods == in.mods);
        r.code = 0u;
        CHECK(!sel_gui_input_map(&st, &r, &in)); /* no code, no event */
        r.kind = SEL_GUI_RAW_KEY;
        CHECK(!sel_gui_input_map(&st, &r, &in));
    }

    return fails == 0 ? 0 : 1;
}
