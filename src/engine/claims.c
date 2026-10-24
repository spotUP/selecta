#include "claims.h"

#include <selecta/plugin.h>

#include <ctype.h>
#include <string.h>

/* True when the comma list names s[0..n) (case-insensitive). */
static int list_has_nocase(const char* list, const char* s, size_t n) {
    while (list != NULL && *list != '\0') {
        const char* e = strchr(list, ',');
        size_t l = e != NULL ? (size_t)(e - list) : strlen(list);

        if (l == n) {
            size_t j = 0;

            while (j < n && tolower((unsigned char)list[j]) == tolower((unsigned char)s[j])) {
                j++;
            }
            if (j == n) {
                return 1;
            }
        }
        list = e != NULL ? e + 1 : NULL;
    }
    return 0;
}

int sel_registry_claims_name(uint32_t (*registry_count)(void), const sel_tag* (*registry_get)(uint32_t), const char* name) {
    const char* first;
    const char* last;
    size_t pre_n, last_n;
    uint32_t i, n;

    if (registry_count == NULL || registry_get == NULL || name == NULL) {
        return 0;
    }
    first = strchr(name, '.');
    if (first == NULL) {
        return 0;
    }
    last = strrchr(name, '.') + 1;
    pre_n = (size_t)(first - name);
    last_n = strlen(last);
    n = registry_count();
    for (i = 0; i < n; i++) {
        const sel_tag* desc = registry_get(i);
        const sel_tag* hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
        const char* exts = hooks != NULL ? (const char*)sel_tag_find(hooks, SEL_RP_EXTENSIONS, 0) : NULL;

        if (exts != NULL && ((pre_n > 0u && list_has_nocase(exts, name, pre_n)) ||
                             (last_n > 0u && list_has_nocase(exts, last, last_n)))) {
            return 1;
        }
    }
    return 0;
}
