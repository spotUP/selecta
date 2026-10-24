/* Terminal character set from the locale variables (locale.h, row I218d). */
#include "locale.h"

#include <ctype.h>
#include <string.h>

static int has_utf8(const char* v) {
    for (; *v != '\0'; v++) {
        size_t n;

        if (toupper((unsigned char)v[0]) != 'U' || toupper((unsigned char)v[1]) != 'T' ||
            toupper((unsigned char)v[2]) != 'F') {
            continue;
        }
        n = v[3] == '-' ? 4u : 3u;
        if (v[n] == '8') return 1;
    }
    return 0;
}

int txt_locale_utf8(const char* lc_all, const char* lc_ctype, const char* lang) {
    if (lc_all != NULL && lc_all[0] != '\0') return has_utf8(lc_all);
    if (lc_ctype != NULL && lc_ctype[0] != '\0') return has_utf8(lc_ctype);
    if (lang != NULL && lang[0] != '\0') return has_utf8(lang);
    return 0;
}
