/* nuade: the player filter and the FORMATS / EXTENSIONS list walk (lists.h), one copy for the plugin and for the host
 * tool that writes the Amiga manifest's lists (I427e4). */
#include "lists.h"

#include <string.h>

int sel_nuade_usable(const NuadePlayer* p) {
    return p != NULL && p->probe != NULL && p->tick != NULL;
}

static int has_token(const char* list, const char* tok, size_t len) {
    const char* q = list;
    while (*q != '\0') {
        const char* e = strchr(q, ',');
        size_t l = e != NULL ? (size_t)(e - q) : strlen(q);
        if (l == len && strncmp(q, tok, len) == 0) {
            return 1;
        }
        q += l + (e != NULL ? 1u : 0u);
    }
    return 0;
}

static int append_token(char* list, const char* tok, size_t len) {
    size_t used = strlen(list);
    if (len == 0u || has_token(list, tok, len)) {
        return 1;
    }
    if (used + (used != 0u ? 1u : 0u) + len + 1u > SEL_NUADE_LIST_LEN) {
        return 0;
    }
    if (used != 0u) {
        list[used++] = ',';
    }
    memcpy(list + used, tok, len);
    list[used + len] = '\0';
    return 1;
}

int sel_nuade_build_lists(char* formats, char* extensions) {
    uint32_t i, n = nuade_player_count();
    formats[0] = extensions[0] = '\0';
    for (i = 0; i < n; i++) {
        const NuadePlayer* p = nuade_player_get(i);
        const char* q;
        if (!sel_nuade_usable(p)) {
            continue;
        }
        if (!append_token(formats, p->id, strlen(p->id))) {
            return 0;
        }
        q = p->extensions != NULL ? p->extensions : "";
        while (*q != '\0') {
            const char* e = strchr(q, ',');
            size_t l = e != NULL ? (size_t)(e - q) : strlen(q);
            if (!append_token(extensions, q, l)) {
                return 0;
            }
            q += l + (e != NULL ? 1u : 0u);
        }
    }
    return 1;
}
