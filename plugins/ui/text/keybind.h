/* Key bindings of the text UI (plan M1.2 phase 5, row I76; design doc section
 * 7). A binding maps one key to one action: a command line sent through the
 * engine (`NEXT`, `SEEK +5000`) or a UI-local action written `@name` (`@quit`,
 * `@palette`). The default table is code; keys.conf lines override it. Pure
 * C99, no allocation. */
#ifndef SELECTA_TEXT_KEYBIND_H
#define SELECTA_TEXT_KEYBIND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TXT_BIND_MAX 128u
#define TXT_BIND_ACTION 80u

/* Where a binding applies. A key looks up its row for the current context
 * first, then its row for any context, so one key can page a list and seek
 * the song (Left, Right). */
typedef enum txt_bind_ctx {
    TXT_CTX_ANY = 0,
    TXT_CTX_LIST = 1,   /* a list fills the main pane: browse, queue, library */
    TXT_CTX_PLAYER = 2, /* the pattern or viz panel fills the main pane */
} txt_bind_ctx;

typedef struct txt_bind {
    uint32_t ctx;  /* txt_bind_ctx */
    uint32_t code; /* txt_key.code */
    uint32_t mods; /* txt_key.mods, TXT_MOD_* */
    char action[TXT_BIND_ACTION];
} txt_bind;

typedef struct txt_bindings {
    txt_bind rows[TXT_BIND_MAX];
    uint32_t count;
} txt_bindings;

/* Replaces the table with the built-in defaults. */
void txt_bindings_default(txt_bindings* b);

/* Applies keys.conf text: lines `<key> <action>`, `unbind <key>`, `# comment`.
 * A key may carry a context prefix, `list:` or `player:` (`player:Left SEEK
 * -5000`); without one the line binds for any context.
 * A bad line (unknown key name, verb not in COMMANDS, unknown @action, table
 * full) is skipped and counted; *first_bad_line (optional) gets the 1-based
 * number of the first one, 0 when none. Returns the number of bad lines. */
uint32_t txt_bindings_load(txt_bindings* b, const char* text, uint32_t* first_bad_line);

/* The action bound to a key in context ctx (txt_bind_ctx): the row for ctx,
 * else the row for any context, else NULL. */
const char* txt_bindings_lookup(const txt_bindings* b, uint32_t ctx, uint32_t code, uint32_t mods);

/* The name of the key bound to an action (exact action first, then any row
 * whose first word is the action), written to out with its context prefix
 * (`player:Left`); NULL when none. */
const char* txt_bindings_key_for(const txt_bindings* b, const char* action, char* out, size_t cap);

/* Key names: a single character, Space Tab Enter Escape Backspace Up Down
 * Left Right Home End PageUp PageDown Insert Delete F1..F12, with C- and M-
 * prefixes. Returns 1 on success. */
int txt_key_parse(const char* name, uint32_t* code, uint32_t* mods);
/* txt_key_parse plus the optional `list:` / `player:` prefix. */
int txt_key_parse_ctx(const char* name, uint32_t* ctx, uint32_t* code, uint32_t* mods);
int txt_key_name(uint32_t code, uint32_t mods, char* out, size_t cap);

/* 1 when `name` (without the @) is a UI-local action. */
int txt_action_known(const char* name);

#ifdef __cplusplus
}
#endif
#endif
