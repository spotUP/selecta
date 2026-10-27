#include "folder_pick.h"

#include <string.h>

int desk_amiga_folder_path(const char* drawer, const char* file, char* out, size_t cap) {
    size_t dl, fl;
    if (drawer == NULL || drawer[0] == '\0' || out == NULL || cap == 0u) {
        return 0;
    }
    dl = strlen(drawer);
    fl = file != NULL ? strlen(file) : 0u;
    if (dl + 1u > cap) {
        return 0;
    }
    memcpy(out, drawer, dl + 1u);
    if (fl > 0u) {
        size_t n = dl;
        if (drawer[dl - 1u] != ':' && drawer[dl - 1u] != '/') {
            if (n + 2u > cap) {
                return 0;
            }
            out[n++] = '/';
        }
        if (n + fl + 1u > cap) {
            return 0;
        }
        memcpy(out + n, file, fl + 1u);
    }
    return 1;
}

int desk_amiga_folder_event(const char* drawer, const char* file, sel_gui_input* ev) {
    char path[SEL_GUI_INPUT_PATH];
    if (ev == NULL || !desk_amiga_folder_path(drawer, file, path, sizeof path)) {
        return 0;
    }
    memset(ev, 0, sizeof *ev);
    ev->kind = SEL_GUI_IN_DROP;
    ev->x = 0; /* the SDL pick queues more = 0 as well: the folder replaces nothing, desktop_ui.c decides */
    memcpy(ev->path, path, strlen(path) + 1u);
    return 1;
}
