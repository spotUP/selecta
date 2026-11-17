/* I1440m: the DeepSID list routes (tags, groups, players, folders, file lists) as rows (design 2026-11-16-i1440-deepsid.md 3.3, 4).
 * Parsing and the cache read live here; the fetch runs on the job thread (art_scrape.c) and fills the 'ds_list' artwork row. */
#include "db_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DL_LIST_CAP 65536u
#define DL_OBJ_MAX 1024u
#define DL_HVSC_PREFIX "_High Voltage SID Collection/"

/* Copies the object starting at *pp ('{') into seg; moves *pp past it. 0 when it is cut off or too long. */
static int take_object(const char** pp, char* seg) {
    const char* p = *pp;
    int depth = 0, quoted = 0;
    size_t n = 0;

    for (; *p != '\0'; p++) {
        if (n + 2 >= DL_OBJ_MAX) return 0;
        seg[n++] = *p;
        if (quoted) {
            if (*p == '\\' && p[1] != '\0') {
                p++;
                seg[n++] = *p;
            } else if (*p == '"') {
                quoted = 0;
            }
        } else if (*p == '"') {
            quoted = 1;
        } else if (*p == '{') {
            depth++;
        } else if (*p == '}' && --depth == 0) {
            seg[n] = '\0';
            *pp = p + 1;
            return 1;
        }
    }
    return 0;
}

static const char* array_start(const char* json) {
    static const char* const wrap[2] = {"\"files\"", "\"tunes\""};
    const char* p;
    uint32_t i;

    for (i = 0; i < 2u; i++) {
        p = strstr(json, wrap[i]);
        p = p != NULL ? sel_json_value(p + strlen(wrap[i])) : NULL;
        if (p != NULL && *p == '[') return p + 1;
    }
    p = strchr(json, '[');
    return p != NULL ? p + 1 : NULL;
}

/* A string or number value of key in one object, as text. */
static void field(const char* seg, const char* key, char* out, size_t cap) {
    char pat[40];
    const char* p;

    out[0] = '\0';
    if (sel_json_str(seg, key, out, cap) != NULL) return;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(seg, pat);
    p = p != NULL ? sel_json_value(p + strlen(pat)) : NULL;
    if (p != NULL && *p >= '0' && *p <= '9') snprintf(out, cap, "%d", atoi(p));
}

int sel_deepsid_list_parse(const char* what, const char* json, sel_deepsid_row_fn fn, void* ctx) {
    char seg[DL_OBJ_MAX], a[256], b[256], c[256], d[64], last[256], num[24];
    const char* p;
    const char* f[4];
    int rows = 0;
    unsigned long grouped = 0;

    if (what == NULL || json == NULL || fn == NULL) return 0;
    p = array_start(json);
    last[0] = '\0';
    while (p != NULL && (p = strchr(p, '{')) != NULL && take_object(&p, seg)) {
        if (strcmp(what, "tags") == 0) { /* name, song count, type, id */
            field(seg, "name", a, sizeof a);
            field(seg, "count", b, sizeof b);
            field(seg, "type", c, sizeof c);
            field(seg, "id", d, sizeof d);
        } else if (strcmp(what, "groups") == 0) { /* one server row per composer, ordered by group name: equal names fold into one row with a count */
            field(seg, "group", a, sizeof a);
            if (a[0] == '\0') continue;
            if (rows > 0 && strcmp(a, last) == 0) {
                grouped++;
                continue;
            }
            if (rows > 0) {
                snprintf(num, sizeof num, "%lu", grouped);
                f[0] = last, f[1] = num, f[2] = "", f[3] = "";
                fn(ctx, f);
            }
            snprintf(last, sizeof last, "%s", a);
            grouped = 1;
            rows++;
            continue;
        } else if (strcmp(what, "players") == 0) { /* title, 0, developer, id */
            field(seg, "title", a, sizeof a);
            snprintf(b, sizeof b, "0");
            field(seg, "developer", c, sizeof c);
            field(seg, "id", d, sizeof d);
        } else if (strcmp(what, "folders") == 0) { /* path, file count, leaf name */
            field(seg, "path", a, sizeof a);
            field(seg, "files", b, sizeof b);
            field(seg, "name", c, sizeof c);
            d[0] = '\0';
        } else { /* files: the song key hvsc:<path>, name, author, player */
            char path[256];
            field(seg, "path", path, sizeof path);
            if (path[0] == '\0') field(seg, "fullname", path, sizeof path);
            if (path[0] == '\0') continue;
            snprintf(a, sizeof a, "hvsc:%s", strncmp(path, DL_HVSC_PREFIX, strlen(DL_HVSC_PREFIX)) == 0 ? path + strlen(DL_HVSC_PREFIX) : path);
            field(seg, "name", b, sizeof b);
            field(seg, "author", c, sizeof c);
            field(seg, "player", d, sizeof d);
        }
        if (a[0] == '\0') continue;
        f[0] = a, f[1] = b, f[2] = c, f[3] = d;
        fn(ctx, f);
        rows++;
    }
    if (strcmp(what, "groups") == 0 && rows > 0) {
        snprintf(num, sizeof num, "%lu", grouped);
        f[0] = last, f[1] = num, f[2] = "", f[3] = "";
        fn(ctx, f);
    }
    return rows;
}

int sel_library_deepsid_list(sel_library* lib, const char* what, const char* arg, sel_deepsid_row_fn fn, void* ctx) {
    char spec[1200];
    char* js;
    sel_art_op op;
    int n, rows;

    if (lib == NULL || what == NULL || fn == NULL) return -1;
    snprintf(spec, sizeof spec, "%s\x1f%s", what, arg != NULL ? arg : "");
    js = malloc(DL_LIST_CAP + 1u);
    if (js == NULL) return -1;
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_GET;
    op.album = sel_art_dz_id(spec);
    op.kind = "ds_list";
    op.data = (uint8_t*)js;
    op.cap = DL_LIST_CAP;
    n = sel_deepsid_art_ask(lib, &op);
    if (n <= 0) {
        sel_library_deepsid_list_kick(lib, spec); /* on the job thread; the next read of the page shows the rows */
        free(js);
        return -1;
    }
    js[n > (int)DL_LIST_CAP ? (int)DL_LIST_CAP : n] = '\0';
    rows = sel_deepsid_list_parse(what, js, fn, ctx);
    free(js);
    return rows;
}
