/* Parser of the Modules.pl search result page (row I406). The page (GET https://www.modules.pl/?id=search&q=<text>)
 * lists every hit in one table row:
 *   <span class="format-small" style="...">mod</span>
 *   <a  href='?id=modules&aid=1100'>Sixtus</a> [& <a ...>Second</a>]
 *   <a class="module" href="?id=module&mod=5601">-- Only chip --</a>
 *   <img src='gfx/play.png' ... onclick='modPlayer("../modules/sixtus/Sixtus_-_--_Only_chip_--.zip", "...", "1")'>
 * The modPlayer argument is the direct file under /modules/. A format the site cannot play has
 * `play-disabled.png` and no such argument. */
#include "modulespl_parse.h"
#include "source/json_scan.h"
#include "source/xml_scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_MARK "<tr valign=\"middle\">"
#define RESULT_MARK "module(s) met your search criteria"
#define ZIP_MARK "modPlayer(\"../modules/"
#define TEXT_MAX 256
#define PATH_CAP 400

/* Copy src[0..n) to out with HTML entities decoded; 0 when it does not fit. */
static int decode(const char* src, size_t n, char* out, size_t cap) {
    size_t o = 0;
    size_t i = 0;

    while (i < n) {
        size_t used = 0, k = src[i] == '&' ? sel_xml_entity(src + i, src + n, out + o, cap > o ? cap - o - 1u : 0, &used) : 0;

        if (k > 0) {
            o += k;
            i += used;
            continue;
        }
        if (o + 1u >= cap) {
            return 0;
        }
        out[o++] = src[i++];
    }
    out[o] = '\0';
    return 1;
}

/* The text between the next '>' at or after p and the following '<', inside [p, end). */
static const char* inner(const char* p, const char* end, size_t* n) {
    const char* gt = memchr(p, '>', (size_t)(end - p));
    const char* lt;

    if (gt == NULL) {
        return NULL;
    }
    gt++;
    lt = memchr(gt, '<', (size_t)(end - gt));
    if (lt == NULL) {
        return NULL;
    }
    *n = (size_t)(lt - gt);
    return gt;
}

static const char* find_in(const char* p, const char* end, const char* needle) {
    size_t nl = strlen(needle);

    while (p + nl <= end) {
        const char* q = memchr(p, needle[0], (size_t)(end - p));

        if (q == NULL || q + nl > end) {
            return NULL;
        }
        if (memcmp(q, needle, nl) == 0) {
            return q;
        }
        p = q + 1;
    }
    return NULL;
}

static int path_ok(const char* path) {
    const char* slash = strchr(path, '/');
    size_t n = strlen(path);
    const char* c;

    if (slash == NULL || slash == path || strchr(slash + 1, '/') != NULL || slash[1] == '\0' || n < 5 ||
        strcmp(path + n - 4, ".zip") != 0 || strstr(path, "..") != NULL) {
        return 0;
    }
    for (c = path; *c != '\0'; c++) {
        if ((unsigned char)*c < 32 || *c == '|' || *c == '"' || *c == '\\' || *c == '?' || *c == '#') {
            return 0;
        }
    }
    return 1;
}

/* 1 delivered, 0 delivered and the receiver wants no more, -1 not a playable row. */
static int one_row(const char* p, const char* end, sel_modulespl_row_fn row, void* ctx) {
    char format[24];
    char artist[TEXT_MAX];
    char title[TEXT_MAX];
    char path[PATH_CAP];
    const char* f = find_in(p, end, "class=\"format-small\"");
    const char* t = find_in(p, end, "href=\"?id=module&mod=");
    const char* z = find_in(p, end, ZIP_MARK);
    const char* a = p;
    const char* zend;
    size_t n = 0;
    size_t al = 0;
    const char* txt;

    if (f == NULL || t == NULL || z == NULL) {
        return -1;
    }
    txt = inner(f, end, &n);
    if (txt == NULL || !decode(txt, n, format, sizeof format)) {
        return -1;
    }
    artist[0] = '\0';
    while ((a = find_in(a, end, "href='?id=modules&aid=")) != NULL && a < t) {
        txt = inner(a, end, &n);
        if (txt == NULL) {
            break;
        }
        if (al > 0 && al + 3u < sizeof artist) {
            memcpy(artist + al, " & ", 4);
            al += 3u;
        }
        if (!decode(txt, n, artist + al, sizeof artist - al)) {
            return -1;
        }
        al = strlen(artist);
        a = txt;
    }
    txt = inner(t, end, &n);
    if (txt == NULL || n == 0 || !decode(txt, n, title, sizeof title)) {
        return -1;
    }
    z += strlen(ZIP_MARK);
    zend = memchr(z, '"', (size_t)(end - z));
    if (zend == NULL || !decode(z, (size_t)(zend - z), path, sizeof path) || !path_ok(path)) {
        return -1;
    }
    return row(ctx, path, title, artist, format) ? 1 : 0;
}

int sel_modulespl_parse(const char* html, size_t len, sel_modulespl_row_fn row, void* ctx) {
    const char* end = html + len;
    const char* p = find_in(html, end, RESULT_MARK);
    int delivered = 0;

    if (p == NULL) {
        return 0;
    }
    p = find_in(p, end, ROW_MARK);
    while (p != NULL) {
        const char* next = find_in(p + 1, end, ROW_MARK);
        const char* stop = next != NULL ? next : end;
        const char* footer = find_in(p, stop, "<table class=\"table-box\">");
        int r;

        if (footer != NULL) {
            stop = footer;
            next = NULL;
        }
        r = one_row(p, stop, row, ctx);
        if (r >= 0) {
            delivered++;
        }
        if (r == 0) {
            break;
        }
        p = next;
    }
    return delivered;
}

/* One UHC row object (the '{' is consumed). */
typedef struct {
    char file[PATH_CAP];
    char dir[PATH_CAP];
    char artist[TEXT_MAX];
    char title[TEXT_MAX];
    char format[24];
} uhc_row;

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
            } else if (strcmp(key, "author(s)") == 0) {
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

int sel_modulespl_parse_uhc(FILE* f, sel_modulespl_row_fn row, void* ctx) {
    static const char prefix[] = "modules/";
    uhc_row u;
    char path[PATH_CAP];
    int delivered = 0;
    int c;

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
        /* UHC path "modules/<author dir>" + filename -> the key "<author dir>/<file>.zip" the HTML parser made. */
        if (strncmp(u.dir, prefix, sizeof prefix - 1u) != 0 ||
            (size_t)snprintf(path, sizeof path, "%s/%s", u.dir + sizeof prefix - 1u, u.file) >= sizeof path ||
            !path_ok(path)) {
            continue;
        }
        delivered++;
        if (!row(ctx, path, u.title, u.artist, u.format)) {
            break;
        }
    }
    return delivered;
}
