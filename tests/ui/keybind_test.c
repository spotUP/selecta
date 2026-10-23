/* T34: key bindings table (plugins/ui/text/keybind.c). Defaults, keys.conf
 * lines, unbind, rejected verbs with line numbers, key names. */
#include "keybind.h"
#include "keys.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                          \
    do {                                                  \
        if (!(c)) {                                       \
            printf("FAIL line %d: %s\n", __LINE__, #c);   \
            g_fail++;                                     \
        }                                                 \
    } while (0)

static const char* look(const txt_bindings* b, uint32_t code, uint32_t mods) {
    return txt_bindings_lookup(b, code, mods);
}

static int is(const char* got, const char* want) {
    return got != NULL && strcmp(got, want) == 0;
}

int main(void) {
    static txt_bindings b;
    uint32_t code, mods, bad = 0;
    char name[32];

    txt_bindings_default(&b);
    /* one row per key the M1.1 switch handled, plus ':' and Ctrl-P */
    CHECK(is(look(&b, 'k', 0), "VIEW_CURSOR -1"));
    CHECK(is(look(&b, 'j', 0), "VIEW_CURSOR +1"));
    CHECK(is(look(&b, TXT_KEY_UP, 0), "VIEW_CURSOR -1"));
    CHECK(is(look(&b, TXT_KEY_DOWN, 0), "VIEW_CURSOR +1"));
    CHECK(is(look(&b, 's', 0), "STOP"));
    CHECK(is(look(&b, 'n', 0), "NEXT"));
    CHECK(is(look(&b, 'p', 0), "PREV"));
    CHECK(is(look(&b, TXT_KEY_LEFT, 0), "SEEK -5000"));
    CHECK(is(look(&b, TXT_KEY_RIGHT, 0), "SEEK +5000"));
    CHECK(is(look(&b, 'a', 0), "@add_file"));
    CHECK(is(look(&b, 'd', 0), "@remove_entry"));
    CHECK(is(look(&b, 'i', 0), "@info"));
    CHECK(is(look(&b, 'q', 0), "@quit"));
    CHECK(is(look(&b, 13, 0), "VIEW_ACTIVATE"));
    CHECK(is(look(&b, ':', 0), "@command"));
    CHECK(is(look(&b, 'p', TXT_MOD_CTRL), "@palette"));
    CHECK(look(&b, 'x', 0) == NULL);

    /* a file line overrides or adds */
    CHECK(txt_bindings_load(&b, "# comment\n\nx QUEUE_SWAP 1 2\nn QUEUE_CLEAR\n", &bad) == 0);
    CHECK(is(look(&b, 'x', 0), "QUEUE_SWAP 1 2"));
    CHECK(is(look(&b, 'n', 0), "QUEUE_CLEAR"));

    /* unbind removes */
    CHECK(txt_bindings_load(&b, "unbind n\nunbind Left\n", &bad) == 0);
    CHECK(look(&b, 'n', 0) == NULL);
    CHECK(look(&b, TXT_KEY_LEFT, 0) == NULL);

    /* a verb not in COMMANDS is rejected with its line number; others still apply */
    bad = 0;
    CHECK(txt_bindings_load(&b, "y NEXT\nz NO_SUCH_VERB 1\nw PREV\n", &bad) == 1);
    CHECK(bad == 2);
    CHECK(look(&b, 'z', 0) == NULL);
    CHECK(is(look(&b, 'y', 0), "NEXT"));
    CHECK(is(look(&b, 'w', 0), "PREV"));
    bad = 0;
    CHECK(txt_bindings_load(&b, "y @no_such_action\n", &bad) == 1 && bad == 1);
    CHECK(txt_bindings_load(&b, "NotAKeyName NEXT\n", &bad) == 1);

    /* key names */
    CHECK(txt_key_parse("F1", &code, &mods) && code == TXT_KEY_F1 && mods == 0);
    CHECK(txt_key_parse("F12", &code, &mods) && code == TXT_KEY_F1 + 11u);
    CHECK(txt_key_parse("C-a", &code, &mods) && code == 'a' && mods == TXT_MOD_CTRL);
    CHECK(txt_key_parse("M-x", &code, &mods) && code == 'x' && mods == TXT_MOD_ALT);
    CHECK(txt_key_parse("Space", &code, &mods) && code == ' ');
    CHECK(txt_key_parse("Tab", &code, &mods) && code == 9);
    CHECK(txt_key_parse("Enter", &code, &mods) && code == 13);
    CHECK(txt_key_parse("Backspace", &code, &mods) && code == 127);
    CHECK(!txt_key_parse("F13", &code, &mods));
    CHECK(!txt_key_parse("", &code, &mods));
    CHECK(txt_key_name('a', TXT_MOD_CTRL, name, sizeof name) && strcmp(name, "C-a") == 0);
    CHECK(txt_key_name(TXT_KEY_F1 + 2u, 0, name, sizeof name) && strcmp(name, "F3") == 0);
    CHECK(txt_bindings_load(&b, "F5 NEXT\nM-x PREV\nC-a STOP\n", &bad) == 0);
    CHECK(is(look(&b, TXT_KEY_F1 + 4u, 0), "NEXT"));
    CHECK(is(look(&b, 'x', TXT_MOD_ALT), "PREV"));
    CHECK(is(look(&b, 'a', TXT_MOD_CTRL), "STOP"));

    /* reverse lookup for the palette */
    txt_bindings_default(&b);
    CHECK(txt_bindings_key_for(&b, "NEXT", name, sizeof name) != NULL && strcmp(name, "n") == 0);
    CHECK(txt_bindings_key_for(&b, "NO_BINDING_FOR_THIS", name, sizeof name) == NULL);

    printf("keybind_test failures=%d\n", g_fail);
    return g_fail != 0;
}
