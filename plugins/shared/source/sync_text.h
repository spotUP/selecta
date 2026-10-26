/* Pure text helpers of the source syncs. Shared by the core syncs (src/library/sources/sync_util.h) and by source
 * plugins (plugins/sources/<id>), so no copy lives in either. */
#ifndef SELECTA_SHARED_SOURCE_SYNC_TEXT_H
#define SELECTA_SHARED_SOURCE_SYNC_TEXT_H

#include <string.h>

/* Strips trailing line ends and spaces in place. */
static inline void sel_sync_chomp(char* s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
        s[--n] = '\0';
    }
}

/* Is `p` a path inside a source (not empty, not absolute, no backslash, no "..")? A song's path can become a file's
 * place on this computer, so a server's list line that names "../x" or "/x" is not taken. */
static inline int sel_sync_safe_path(const char* p) {
    return p[0] != '\0' && p[0] != '/' && strchr(p, '\\') == NULL && strstr(p, "..") == NULL;
}

#endif
