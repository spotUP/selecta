/* Terminal input parser, plan M1.1 phase 5 (row I59). */
#include "keys.h"

#include <string.h>

enum { S_GROUND, S_ESC, S_CSI, S_SS3, S_UTF8 };

void txt_keys_init(txt_keys* k) { memset(k, 0, sizeof *k); }

static void emit(txt_keys* k, txt_key* out, uint32_t code, uint32_t mods) {
    memset(out, 0, sizeof *out);
    out->code = code;
    out->mods = mods | (k->alt ? TXT_MOD_ALT : 0u);
    k->alt = 0;
    k->state = S_GROUND;
}

static uint32_t xterm_mods(uint32_t p) {
    uint32_t m = 0;
    if (p < 2) return 0;
    p -= 1;
    if (p & 1u) m |= TXT_MOD_SHIFT;
    if (p & 2u) m |= TXT_MOD_ALT;
    if (p & 4u) m |= TXT_MOD_CTRL;
    return m;
}

static uint32_t final_key(uint8_t f) {
    switch (f) {
    case 'A': return TXT_KEY_UP;
    case 'B': return TXT_KEY_DOWN;
    case 'C': return TXT_KEY_RIGHT;
    case 'D': return TXT_KEY_LEFT;
    case 'H': return TXT_KEY_HOME;
    case 'F': return TXT_KEY_END;
    case 'P': return TXT_KEY_F1;      /* SS3 P..S: F1..F4 */
    case 'Q': return TXT_KEY_F1 + 1u;
    case 'R': return TXT_KEY_F1 + 2u;
    case 'S': return TXT_KEY_F1 + 3u;
    default: return 0;
    }
}

static uint32_t tilde_key(uint32_t n) {
    switch (n) {
    case 1:
    case 7: return TXT_KEY_HOME;
    case 2: return TXT_KEY_INSERT;
    case 3: return TXT_KEY_DELETE;
    case 4:
    case 8: return TXT_KEY_END;
    case 5: return TXT_KEY_PAGE_UP;
    case 6: return TXT_KEY_PAGE_DOWN;
    case 11: case 12: case 13: case 14: case 15: return TXT_KEY_F1 + (n - 11u); /* F1..F5 */
    case 17: case 18: case 19: case 20: case 21: return TXT_KEY_F1 + (n - 12u); /* F6..F10 */
    case 23: case 24: return TXT_KEY_F1 + (n - 13u); /* F11, F12 */
    default: return 0;
    }
}

/* The Amiga console.device sends keys after the 8-bit CSI 0x9B, with its own codes (Amiga ROM
 * Kernel Reference Manual: Devices, console.device input): Shift+Up is CSI T, Shift+Down CSI S,
 * Shift+Left CSI space A, Shift+Right CSI space @. 0x9B is never a UTF-8 lead byte, so taking it as
 * the CSI costs the xterm and UTF-8 input nothing. */
static uint32_t amiga_final(uint8_t f, int space, uint32_t* mods) {
    *mods = 0;
    if (space) {
        *mods = TXT_MOD_SHIFT;
        return f == 'A' ? TXT_KEY_LEFT : f == '@' ? TXT_KEY_RIGHT : 0u;
    }
    switch (f) {
    case 'A': return TXT_KEY_UP;
    case 'B': return TXT_KEY_DOWN;
    case 'C': return TXT_KEY_RIGHT;
    case 'D': return TXT_KEY_LEFT;
    case 'T': *mods = TXT_MOD_SHIFT; return TXT_KEY_UP;
    case 'S': *mods = TXT_MOD_SHIFT; return TXT_KEY_DOWN;
    default: return 0;
    }
}

/* Amiga console CSI n ~: F1..F10 are 0..9, Shift+F1..F10 10..19; the PC keys of a 105-key
 * keymap are Insert 40, Page Up 41, Page Down 42, Home 44, End 45, and 50.. with Shift. */
static uint32_t amiga_tilde(uint32_t n, uint32_t* mods) {
    *mods = 0;
    if (n >= 10 && n <= 19) {
        *mods = TXT_MOD_SHIFT;
        n -= 10;
    } else if (n >= 50 && n <= 55) {
        *mods = TXT_MOD_SHIFT;
        n -= 10;
    }
    switch (n) {
    case 40: return TXT_KEY_INSERT;
    case 41: return TXT_KEY_PAGE_UP;
    case 42: return TXT_KEY_PAGE_DOWN;
    case 44: return TXT_KEY_HOME;
    case 45: return TXT_KEY_END;
    default: return n <= 9 ? TXT_KEY_F1 + n : 0u;
    }
}

static void csi_begin(txt_keys* k, int amiga) {
    k->state = S_CSI;
    k->nparams = 0;
    memset(k->params, 0, sizeof k->params);
    k->priv = 0;
    k->amiga = amiga;
    k->space = 0;
}

static int ground(txt_keys* k, uint8_t b, txt_key* out) {
    if (b == 0x1B) {
        k->state = S_ESC;
        return 0;
    }
    if (b == 0x9B) {
        csi_begin(k, 1);
        return 0;
    }
    if (b == 13 || b == 10) {
        emit(k, out, 13, 0);
        return 1;
    }
    if (b == 9 || b == 127 || b == 8) {
        emit(k, out, b == 8 ? 127u : b, 0);
        return 1;
    }
    if (b < 0x20) { /* Ctrl+A is 1 */
        emit(k, out, (uint32_t)b + 'a' - 1u, TXT_MOD_CTRL);
        return 1;
    }
    if (b < 0x80) {
        emit(k, out, b, 0);
        return 1;
    }
    if (b >= 0xC2 && b < 0xE0) {
        k->utf8_left = 1;
        k->utf8_cp = b & 0x1Fu;
    } else if (b >= 0xE0 && b < 0xF0) {
        k->utf8_left = 2;
        k->utf8_cp = b & 0x0Fu;
    } else if (b >= 0xF0 && b < 0xF5) {
        k->utf8_left = 3;
        k->utf8_cp = b & 0x07u;
    } else {
        emit(k, out, 0xFFFD, 0);
        return 1;
    }
    k->state = S_UTF8;
    return 0;
}

int txt_keys_feed(txt_keys* k, uint8_t b, txt_key* out) {
    switch (k->state) {
    case S_UTF8:
        if ((b & 0xC0u) != 0x80u) {
            emit(k, out, 0xFFFD, 0);
            return 1;
        }
        k->utf8_cp = (k->utf8_cp << 6) | (b & 0x3Fu);
        if (--k->utf8_left == 0) {
            emit(k, out, k->utf8_cp, 0);
            return 1;
        }
        return 0;
    case S_ESC:
        if (b == '[') {
            csi_begin(k, 0);
            return 0;
        }
        if (b == 'O') {
            k->state = S_SS3;
            return 0;
        }
        if (b == 0x1B) { /* ESC ESC: the first is a real Escape */
            emit(k, out, 27, 0);
            k->state = S_ESC;
            return 1;
        }
        k->alt = 1; /* ESC prefix means Alt */
        k->state = S_GROUND;
        return ground(k, b, out);
    case S_SS3: {
        uint32_t code = final_key(b);
        if (code) {
            emit(k, out, code, 0);
            return 1;
        }
        k->state = S_GROUND;
        return 0;
    }
    case S_CSI:
        if (b == '<') {
            k->priv = 1;
            return 0;
        }
        if (b == ' ' && k->amiga) {
            k->space = 1;
            return 0;
        }
        if (b >= '0' && b <= '9') {
            if (k->nparams == 0) k->nparams = 1;
            k->params[k->nparams - 1] = k->params[k->nparams - 1] * 10u + (uint32_t)(b - '0');
            return 0;
        }
        if (b == ';') {
            if (k->nparams == 0) k->nparams = 1;
            if (k->nparams < 4) k->nparams++;
            return 0;
        }
        if (b >= 0x40 && b <= 0x7E) {
            uint32_t code;
            if (k->priv && (b == 'M' || b == 'm') && k->nparams >= 3) {
                uint32_t bt = k->params[0];
                uint32_t mods = ((bt & 4u) ? TXT_MOD_SHIFT : 0u) | ((bt & 8u) ? TXT_MOD_ALT : 0u) |
                                ((bt & 16u) ? TXT_MOD_CTRL : 0u);
                emit(k, out, TXT_KEY_MOUSE, mods);
                out->button = bt & ~(4u | 8u | 16u);
                out->x = k->params[1] ? k->params[1] - 1u : 0u;
                out->y = k->params[2] ? k->params[2] - 1u : 0u;
                out->pressed = (b == 'M');
                return 1;
            }
            if (k->amiga) {
                uint32_t mods;
                code = (b == '~') ? (k->nparams > 0 ? amiga_tilde(k->params[0], &mods) : 0u)
                                  : amiga_final(b, k->space, &mods);
                if (code) {
                    emit(k, out, code, mods);
                    return 1;
                }
            } else {
                code = (b == '~') ? tilde_key(k->params[0]) : final_key(b);
                if (code) {
                    emit(k, out, code, k->nparams > 1 ? xterm_mods(k->params[1]) : 0u);
                    return 1;
                }
            }
            k->state = S_GROUND; /* unknown sequence: dropped whole */
            k->alt = 0;
        }
        return 0;
    default:
        return ground(k, b, out);
    }
}

int txt_keys_timeout(txt_keys* k, txt_key* out) {
    if (k->state != S_ESC) return 0;
    emit(k, out, 27, 0);
    return 1;
}
