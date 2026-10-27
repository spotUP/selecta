/* Unit test of the Amiga folder dialog's platform independent half (I531): what an asl.library drawers-only requester
 * answers becomes the SEL_GUI_IN_DROP that desktop_ui.c turns into LIBRARY_SCAN; cancel yields nothing. */
#include "amiga/folder_pick.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c) \
    do { \
        if (!(c)) { \
            fprintf(stderr, "amiga_folder_pick_unit: line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static void expect_drop(const char* drawer, const char* file, const char* want) {
    sel_gui_input ev;
    memset(&ev, 0x5a, sizeof ev);
    CHECK(desk_amiga_folder_event(drawer, file, &ev) == 1);
    CHECK(ev.kind == SEL_GUI_IN_DROP);
    CHECK(ev.x == 0);
    CHECK(strcmp(ev.path, want) == 0);
}

int main(void) {
    sel_gui_input ev;
    char buf[8];
    expect_drop("Work:Music/My Mods", "", "Work:Music/My Mods");
    expect_drop("Work:Music/My Mods", NULL, "Work:Music/My Mods");
    expect_drop("Work:", "", "Work:");
    expect_drop("Work:Music", "Chip Tunes", "Work:Music/Chip Tunes"); /* AddPart joins with a slash */
    expect_drop("Work:", "Mods", "Work:Mods");                         /* a volume root takes none */
    expect_drop("Work:Music/", "Mods", "Work:Music/Mods");
    /* cancel: AslRequest returned FALSE or the drawer is empty; nothing is queued, ev stays untouched */
    memset(&ev, 0x5a, sizeof ev);
    CHECK(desk_amiga_folder_event(NULL, NULL, &ev) == 0);
    CHECK(desk_amiga_folder_event("", "x", &ev) == 0);
    CHECK(((unsigned char*)&ev)[0] == 0x5a);
    /* too long for the event path: refused, not truncated into a different folder */
    {
        char big[SEL_GUI_INPUT_PATH + 8];
        memset(big, 'a', sizeof big - 1u);
        big[sizeof big - 1u] = '\0';
        CHECK(desk_amiga_folder_event(big, "", &ev) == 0);
    }
    CHECK(desk_amiga_folder_path("abc", "d", buf, 4u) == 0);
    CHECK(desk_amiga_folder_path("ab:", "cd", buf, sizeof buf) == 1 && strcmp(buf, "ab:cd") == 0);
    if (fails != 0) {
        return 1;
    }
    printf("amiga_folder_pick_unit: ok\n");
    return 0;
}
