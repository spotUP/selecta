/* AmigaOS program launcher for the command line's `!`, plan M2 phase 6 (row I131). */
#include "../spawn.h"

#include <dos/dos.h>
#include <proto/dos.h>

#include <string.h>

#define SPAWN_LINE 512u

int txt_spawn(const char* line, const char* const* paths, uint32_t n) {
    char cmd[SPAWN_LINE];
    size_t len = strlen(line);
    uint32_t i;
    LONG rc;
    if (len == 0 || len >= sizeof cmd) return -1;
    memcpy(cmd, line, len + 1u);
    for (i = 0; i < n; i++) {
        size_t ql;
        if (len + 2u >= sizeof cmd) return -1;
        cmd[len++] = ' ';
        ql = txt_quote_arg(cmd + len, sizeof cmd - len, paths[i]);
        if (ql == 0) return -1;
        len += ql;
    }
    rc = SystemTagList(cmd, NULL);
    return rc == -1 ? -1 : (int)rc; /* -1: the command could not be started */
}
