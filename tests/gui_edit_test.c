/* I1243: the field edit core: select, replace, delete, word, copy. */
#include "selecta/gui/gui_edit.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    char buf[32] = "hello big world", out[32];
    size_t len = strlen(buf);
    sel_gui_edit e = {0u, 0u, 0u};
    sel_gui_edit_sync(&e, len);
    CHECK(e.caret == len && !sel_gui_edit_has(&e));
    sel_gui_edit_word(&e, buf, len, 7u); /* inside "big" */
    CHECK(e.anchor == 6u && e.caret == 9u);
    CHECK(sel_gui_edit_copy(&e, buf, out, sizeof out) == 3u && strcmp(out, "big") == 0);
    CHECK(sel_gui_edit_insert(&e, buf, &len, sizeof buf, 0, "X", 1u) && strcmp(buf, "hello X world") == 0 && e.caret == 7u);
    sel_gui_edit_all(&e, len);
    CHECK(sel_gui_edit_copy(&e, buf, out, sizeof out) == len);
    CHECK(sel_gui_edit_insert(&e, buf, &len, sizeof buf, 0, "ab", 2u) && strcmp(buf, "ab") == 0); /* typing over Select All */
    sel_gui_edit_all(&e, len);
    CHECK(sel_gui_edit_backspace(&e, buf, &len) && len == 0u && buf[0] == '\0');
    CHECK(!sel_gui_edit_backspace(&e, buf, &len));
    sel_gui_edit_insert(&e, buf, &len, sizeof buf, '"', "a\"bc", 4u); /* reject byte dropped */
    CHECK(strcmp(buf, "abc") == 0);
    sel_gui_edit_move(&e, len, -1, 1); /* Shift+Left extends from the end */
    sel_gui_edit_move(&e, len, -1, 1);
    CHECK(e.anchor == 3u && e.caret == 1u);
    sel_gui_edit_move(&e, len, 1, 0); /* a plain Right collapses to the selection's right edge */
    CHECK(!sel_gui_edit_has(&e) && e.caret == 3u);
    sel_gui_edit_move(&e, len, -1, 0);
    CHECK(sel_gui_edit_backspace(&e, buf, &len) && strcmp(buf, "ac") == 0); /* backspace before a mid caret */
    len = 1u; /* the buffer changed behind the state: the caret goes to the end */
    sel_gui_edit_sync(&e, len);
    CHECK(e.caret == 1u && e.anchor == 1u);
    if (fails == 0) {
        printf("gui_edit_ok\n");
    }
    return fails != 0;
}
