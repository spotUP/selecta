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
        size_t pl = strlen(paths[i]);
        if (len + pl + 4u >= sizeof cmd) return -1;
        cmd[len++] = ' ';
        cmd[len++] = '"';
        memcpy(cmd + len, paths[i], pl);
        len += pl;
        cmd[len++] = '"';
        cmd[len] = '\0';
    }
    rc = SystemTagList(cmd, NULL);
    return rc == -1 ? -1 : (int)rc; /* -1: the command could not be started */
}
