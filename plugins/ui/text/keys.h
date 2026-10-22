/* Terminal input parser for the text UI: bytes in, key events out
 * (xterm-256color, UTF-8, SGR mouse). Plan M1.1 phase 5, row I59. Pure C99. */
#ifndef SELECTA_TEXT_KEYS_H
#define SELECTA_TEXT_KEYS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Codes below 0x110000 are Unicode code points (Enter is 13, Tab 9, Escape 27,
 * Backspace 127). A control byte arrives as its letter with TXT_MOD_CTRL, e.g.
 * Ctrl+C is code 'c' with TXT_MOD_CTRL. */
enum {
    TXT_KEY_UP = 0x110000,
    TXT_KEY_DOWN,
    TXT_KEY_RIGHT,
    TXT_KEY_LEFT,
    TXT_KEY_HOME,
    TXT_KEY_END,
    TXT_KEY_PAGE_UP,
    TXT_KEY_PAGE_DOWN,
    TXT_KEY_INSERT,
    TXT_KEY_DELETE,
    TXT_KEY_MOUSE
};

#define TXT_MOD_SHIFT 1u
#define TXT_MOD_ALT 2u
#define TXT_MOD_CTRL 4u

typedef struct txt_key {
    uint32_t code;
    uint32_t mods;
    uint32_t x, y;   /* mouse only, zero based */
    uint32_t button; /* mouse only: 0 left, 1 middle, 2 right, 64 wheel up, 65 wheel down */
    int pressed;     /* mouse only: 1 press, 0 release */
} txt_key;

typedef struct txt_keys {
    int state;
    uint32_t params[4];
    uint32_t nparams;
    uint32_t utf8_left;
    uint32_t utf8_cp;
    int priv; /* '<' seen: SGR mouse */
    int alt;  /* ESC prefix pending for the next key */
} txt_keys;

void txt_keys_init(txt_keys* k);
/* Feeds one byte; returns 1 and fills *out when a key is complete, else 0. */
int txt_keys_feed(txt_keys* k, uint8_t byte, txt_key* out);
/* Call when input went quiet: a lone pending ESC becomes the Escape key.
 * Returns 1 and fills *out when one was pending. */
int txt_keys_timeout(txt_keys* k, txt_key* out);

#ifdef __cplusplus
}
#endif
#endif
