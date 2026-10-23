/* AmigaDOS command-line quoting for txt_spawn (plan M2 phase 6, row I131). Pure
 * string code so the desktop unit tests cover what the Amiga launcher builds. */
#include "spawn.h"

#include <string.h>

size_t txt_quote_arg(char* out, size_t cap, const char* s) {
    size_t n = 0;
    if (cap < 3u) {
        return 0;
    }
    out[n++] = '"';
    for (; *s; s++) {
        size_t need = (*s == '"' || *s == '*') ? 2u : 1u;
        if (n + need + 2u > cap) { /* closing quote and NUL must still fit */
            return 0;
        }
        if (need == 2u) {
            out[n++] = '*'; /* AmigaDOS escape character */
        }
        out[n++] = *s;
    }
    out[n++] = '"';
    out[n] = '\0';
    return n;
}
