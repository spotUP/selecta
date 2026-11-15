/* The UHC answer reader (row I1405, shared since I1407). Pure: no host. */
#include "source/uhc.h"
#include "source/json_scan.h"

#include <string.h>

#define PATH_CAP 400
#define TEXT_MAX 256

typedef struct {
    char file[PATH_CAP];
    char dir[PATH_CAP];
    char artist[TEXT_MAX];
    char title[TEXT_MAX];
    char format[24];
} uhc_row;

/* One row object (the '{' is consumed). */
static int uhc_read_row(FILE* f, uhc_row* u) {
    char key[24];
    int c;

    memset(u, 0, sizeof *u);
    for (;;) {
        char* dst = NULL;
        size_t cap = 0;
        int end = 0;

        c = sel_json_skip_ws(f);
        if (c == '}') {
            return 1;
        }
        if (c == ',') {
            continue;
        }
        if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
            return 0;
        }
        c = sel_json_skip_ws(f);
        if (c == '"') {
            if (strcmp(key, "filename") == 0) {
                dst = u->file, cap = sizeof u->file;
            } else if (strcmp(key, "path") == 0) {
                dst = u->dir, cap = sizeof u->dir;
            } else if (strcmp(key, "author(s)") == 0 || strcmp(key, "artist(s)") == 0) {
                dst = u->artist, cap = sizeof u->artist;
            } else if (strcmp(key, "title") == 0) {
                dst = u->title, cap = sizeof u->title;
            } else if (strcmp(key, "format") == 0) {
                dst = u->format, cap = sizeof u->format;
            }
            if (!sel_json_read_string(f, dst, cap)) {
                return 0;
            }
            c = sel_json_skip_ws(f);
            if (c == '}') {
                return 1;
            }
        } else {
            if (!sel_json_skip_value(f, c, &end)) {
                return 0;
            }
            if (end == '}') {
                return 1;
            }
        }
    }
}

int sel_uhc_parse(FILE* f, sel_uhc_row_fn row, void* ctx) {
    uhc_row u;
    int delivered = 0;
    int c;
    int r;

    if (sel_json_skip_ws(f) != '[') {
        return -1;
    }
    while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
        if (c == ',') {
            continue;
        }
        if (c != '{' || !uhc_read_row(f, &u)) {
            return delivered > 0 ? delivered : -1;
        }
        r = row(ctx, u.dir, u.file, u.title, u.artist, u.format);
        delivered += r >= 0;
        if (r == 0) {
            break;
        }
    }
    return delivered;
}
