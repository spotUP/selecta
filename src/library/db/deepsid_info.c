/* I1440k: Song Information rows from the DeepSID 'ds_file' answer (design 2026-11-16-i1440-deepsid.md 3.2). Parsing lives here; the views only draw the rows. */
#include "db_internal.h"

#include "../song_key.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DS_ROW_MAX 256u
#define DS_PLACINGS_MAX 6u
#define DS_TAG_GROUPS 5u
#define DS_JSON_CAP 65536u

/* Appends one pair before the closing empty label; 0 when value is empty or it does not fit. */
static int put(char* out, uint32_t cap, const char* label, const char* value) {
    uint32_t used = 0, ll = (uint32_t)strlen(label) + 1u, lv;

    if (value == NULL || value[0] == '\0') return 0;
    lv = (uint32_t)strlen(value) + 1u;
    while (out[used] != '\0') {
        used += (uint32_t)strlen(out + used) + 1u;
        used += (uint32_t)strlen(out + used) + 1u;
    }
    if (used + ll + lv + 1u > cap) return 0;
    memcpy(out + used, label, ll);
    memcpy(out + used + ll, value, lv);
    out[used + ll + lv] = '\0';
    return 1;
}

static int ci_has(const char* s, const char* sub) {
    size_t n = strlen(sub), i, j;

    for (i = 0; s[i] != '\0'; i++) {
        for (j = 0; j < n && s[i + j] != '\0'; j++) {
            char a = s[i + j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (a != sub[j]) break;
        }
        if (j == n) return 1;
    }
    return 0;
}

static int num_of(const char* seg, const char* key) {
    char pat[32];
    const char* p;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(seg, pat);
    p = p != NULL ? sel_json_value(p + strlen(pat)) : NULL;
    return p != NULL ? atoi(p) : 0;
}

static void tags(const char* json, char* out, uint32_t cap, uint32_t* added) {
    static const char* const label[DS_TAG_GROUPS] = {"Genre tags", "Production tags", "Origin tags", "Digi tags", "Other tags"};
    static const char* const word[4] = {"genre", "production", "origin", "digi"};
    char list[DS_TAG_GROUPS][DS_ROW_MAX], name[96], type[32], seg[256];
    const char *p = strstr(json, "\"tags\""), *end, *obj, *close;
    unsigned g, k;
    size_t n;

    memset(list, 0, sizeof list);
    p = p != NULL ? sel_json_value(p + 6) : NULL;
    end = p != NULL && *p == '[' ? strchr(p, ']') : NULL;
    while (end != NULL && (obj = strchr(p, '{')) != NULL && obj < end && (close = strchr(obj, '}')) != NULL) {
        n = (size_t)(close - obj) < sizeof seg - 1u ? (size_t)(close - obj) : sizeof seg - 1u;
        memcpy(seg, obj, n);
        seg[n] = '\0';
        p = close + 1;
        if (sel_json_str(seg, "name", name, sizeof name) == NULL) continue;
        type[0] = '\0';
        sel_json_str(seg, "type", type, sizeof type);
        for (g = 0; g < 4u; g++) {
            if (ci_has(type, word[g])) break; /* SUBORIGIN, SUBDIGI and MIXORIGIN land with their parent */
        }
        k = (unsigned)strlen(list[g]);
        snprintf(list[g] + k, DS_ROW_MAX - k, "%s%s", k > 0 ? ", " : "", name);
    }
    for (g = 0; g < DS_TAG_GROUPS; g++) *added += (uint32_t)put(out, cap, label[g], list[g]);
}

static void placings(const char* json, char* out, uint32_t cap, uint32_t* added) {
    static const char* const ord[] = {"th", "st", "nd", "rd"};
    const char *p = strstr(json, "\"placings\""), *end, *ev, *next;
    char seg[512], comp[96], name[96], row[DS_ROW_MAX];
    uint32_t n = 0;
    size_t len;
    int place, year;

    p = p != NULL ? sel_json_value(p + 10) : NULL;
    end = p != NULL && *p == '[' ? strchr(p, ']') : NULL; /* shortcut: a ']' inside a competition name ends the list early; upgrade if the server ever sends one */
    while (end != NULL && n < DS_PLACINGS_MAX && (ev = strstr(p, "\"eventId\"")) != NULL && ev < end) {
        next = strstr(ev + 9, "\"eventId\"");
        len = next != NULL && next < end ? (size_t)(next - ev) : (size_t)(end - ev);
        if (len >= sizeof seg) len = sizeof seg - 1u;
        memcpy(seg, ev, len);
        seg[len] = '\0';
        p = ev + 9;
        place = num_of(seg, "place");
        year = num_of(seg, "year");
        comp[0] = name[0] = '\0';
        sel_json_str(seg, "competition", comp, sizeof comp);
        sel_json_str(seg, "name", name, sizeof name);
        if (place <= 0 || (comp[0] == '\0' && name[0] == '\0')) continue;
        snprintf(row, sizeof row, "%d%s", place, (place % 100 >= 11 && place % 100 <= 13) || place % 10 > 3 ? ord[0] : ord[place % 10]);
        if (comp[0] != '\0') snprintf(row + strlen(row), sizeof row - strlen(row), " at %s", comp);
        if (year > 0) snprintf(row + strlen(row), sizeof row - strlen(row), " %d", year);
        if (name[0] != '\0') snprintf(row + strlen(row), sizeof row - strlen(row), ", %s", name);
        *added += (uint32_t)put(out, cap, "Compo placing", row);
        n++;
    }
}

uint32_t sel_deepsid_rows(const char* json, char* out, uint32_t cap, uint32_t* rows) {
    char a[96], b[96], c[96], row[DS_ROW_MAX];
    uint32_t added = 0;

    if (json == NULL || out == NULL) return 0;
    tags(json, out, cap, &added);
    a[0] = b[0] = '\0';
    sel_json_str(json, "player", a, sizeof a);
    sel_json_str(json, "playerType", b, sizeof b);
    snprintf(row, sizeof row, "%s%s%s", a, a[0] != '\0' && b[0] != '\0' ? ", " : "", b);
    added += (uint32_t)put(out, cap, "Player", row);
    a[0] = '\0';
    sel_json_str(json, "application", a, sizeof a);
    added += (uint32_t)put(out, cap, "Editor", a);
    a[0] = b[0] = '\0';
    sel_json_str(json, "sidModel", a, sizeof a);
    sel_json_str(json, "clockSpeed", b, sizeof b);
    added += (uint32_t)put(out, cap, "Chip", a);
    added += (uint32_t)put(out, cap, "Clock", b);
    a[0] = b[0] = c[0] = '\0';
    sel_json_str(json, "loadAddr", a, sizeof a);
    sel_json_str(json, "initAddr", b, sizeof b);
    sel_json_str(json, "playAddr", c, sizeof c);
    if (a[0] != '\0' || b[0] != '\0' || c[0] != '\0') {
        snprintf(row, sizeof row, "Load %s, init %s, play %s", a[0] != '\0' ? a : "-", b[0] != '\0' ? b : "-", c[0] != '\0' ? c : "-");
        added += (uint32_t)put(out, cap, "Technical", row);
    }
    placings(json, out, cap, &added);
    a[0] = '\0';
    sel_json_str(json, "gb64", a, sizeof a); /* a JSON null never matches: the scanner wants a quote */
    added += (uint32_t)put(out, cap, "GB64 game", a);
    if (rows != NULL) *rows += added;
    return added;
}

int sel_deepsid_art_ask(sel_library* lib, const sel_art_op* op) {
    sel_message reply;

    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(lib, SEL_LIB_REQ_ART, op, &reply) || reply.argc < 1) return 0;
    return reply.args[0].v.i;
}

uint32_t sel_library_deepsid_panel(sel_library* lib, const char* ref, char* out, uint32_t cap, uint32_t* rows) {
    char key[1100];
    char* js;
    sel_art_op op;
    uint32_t added = 0;
    int n;

    if (lib == NULL || ref == NULL || strncmp(ref, "hvsc:", 5) != 0) return 0; /* shortcut: HVSC keys only; CGSC and md5 matches follow when the key mapper is wired here */
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_DS_KEY;
    op.ref = ref;
    op.data = (uint8_t*)key;
    op.cap = sizeof key;
    if (sel_deepsid_art_ask(lib, &op) <= 0) return 0;
    js = malloc(DS_JSON_CAP + 1u);
    if (js == NULL) return 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_GET;
    op.album = sel_art_dz_id(key);
    op.kind = "ds_file";
    op.data = (uint8_t*)js;
    op.cap = DS_JSON_CAP;
    n = sel_deepsid_art_ask(lib, &op);
    if (n > 0) {
        js[n > (int)DS_JSON_CAP ? (int)DS_JSON_CAP : n] = '\0';
        added = sel_deepsid_rows(js, out, cap, rows);
    } else {
        sel_library_deepsid_kick(lib, ref); /* queued on the job thread; the next panel read shows the rows */
    }
    free(js);
    return added;
}

/* I1440j: the composer page header (design 3.1) from the DeepSID 'ds_composer' answer, as label/value pairs like sel_deepsid_rows. */
static void composer_list(const char* json, const char* key, const char* label, char* out, uint32_t cap, uint32_t* added) {
    char pat[32], list[DS_ROW_MAX];
    const char *p, *end, *q;
    size_t n, k = 0;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(json, pat);
    p = p != NULL ? sel_json_value(p + strlen(pat)) : NULL;
    end = p != NULL && *p == '[' ? strchr(p, ']') : NULL;
    list[0] = '\0';
    while (end != NULL && (p = strchr(p + 1, '"')) != NULL && p < end) {
        q = strchr(p + 1, '"');
        if (q == NULL || q > end) break;
        n = (size_t)(q - p - 1);
        if (k + n + 3u >= sizeof list) break;
        k += (size_t)snprintf(list + k, sizeof list - k, "%s%.*s", k > 0 ? ", " : "", (int)n, p + 1);
        p = q;
    }
    *added += (uint32_t)put(out, cap, label, list);
}

static void composer_objs(const char* json, const char* key, const char* a_key, const char* b_key, char* out, uint32_t cap, uint32_t* added, int links) {
    char pat[32], seg[384], a[96], b[160], row[DS_ROW_MAX];
    const char *p, *end, *obj, *close;
    uint32_t n = 0;
    size_t len;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(json, pat);
    p = p != NULL ? sel_json_value(p + strlen(pat)) : NULL;
    end = p != NULL && *p == '[' ? strchr(p, ']') : NULL;
    while (end != NULL && n < 12u && (obj = strchr(p, '{')) != NULL && obj < end && (close = strchr(obj, '}')) != NULL) {
        len = (size_t)(close - obj) < sizeof seg - 1u ? (size_t)(close - obj) : sizeof seg - 1u;
        memcpy(seg, obj, len);
        seg[len] = '\0';
        p = close + 1;
        a[0] = b[0] = '\0';
        sel_json_str(seg, a_key, a, sizeof a);
        sel_json_str(seg, b_key, b, sizeof b);
        if (a[0] == '\0') continue;
        if (links) snprintf(row, sizeof row, "%s: %s", a, b);
        else snprintf(row, sizeof row, "%s%s%s", a, b[0] != '\0' ? ", " : "", b);
        *added += (uint32_t)put(out, cap, links ? "Link" : "Worked at", row);
        n++;
    }
}

static void composer_str(const char* json, const char* key, const char* label, char* out, uint32_t cap, uint32_t* added) {
    char v[DS_ROW_MAX];

    v[0] = '\0';
    sel_json_str(json, key, v, sizeof v);
    *added += (uint32_t)put(out, cap, label, v);
}

uint32_t sel_deepsid_composer_rows(const char* json, char* out, uint32_t cap) {
    char v[DS_ROW_MAX], w[96];
    const char* c;
    uint32_t added = 0;
    int id;

    if (json == NULL || out == NULL || cap < 2u || strstr(json, "\"found\":false") != NULL) return 0;
    out[0] = out[1] = '\0';
    composer_str(json, "name", "Name", out, cap, &added);
    composer_list(json, "handles", "Handles", out, cap, &added);
    composer_str(json, "born", "Born", out, cap, &added);
    composer_str(json, "died", "Died", out, cap, &added);
    composer_str(json, "country", "Country", out, cap, &added);
    composer_str(json, "focus", "Focus", out, cap, &added);
    composer_str(json, "affiliation", "Groups", out, cap, &added);
    composer_objs(json, "employment", "company", "years", out, cap, &added, 0);
    composer_str(json, "notable", "Notable", out, cap, &added);
    composer_objs(json, "links", "name", "url", out, cap, &added, 1);
    id = num_of(json, "csdbId");
    if (id > 0) {
        snprintf(v, sizeof v, "CSDb: https://csdb.dk/scener/?id=%d", id);
        added += (uint32_t)put(out, cap, "Link", v);
    }
    c = strstr(json, "\"counts\"");
    if (c != NULL) {
        w[0] = '\0';
        sel_json_str(c, "collection", w, sizeof w);
        snprintf(v, sizeof v, "%d", num_of(c, "files"));
        added += (uint32_t)put(out, cap, strcmp(w, "CGSC") == 0 ? "Songs in CGSC" : "Songs in HVSC", v);
        snprintf(v, sizeof v, "%d", num_of(c, "placings"));
        added += (uint32_t)put(out, cap, "Compo placings", v);
        snprintf(v, sizeof v, "%d", num_of(c, "youtube"));
        added += (uint32_t)put(out, cap, "YouTube captures", v);
    }
    return added;
}

/* The header rows for the composer of song `ref` (any hvsc: song of the page): the ds_composer row of its folder; none cached yet kicks the fetcher. */
uint32_t sel_library_deepsid_composer(sel_library* lib, const char* ref, char* out, uint32_t cap) {
    char key[1100];
    char *js, *slash;
    sel_art_op op;
    uint32_t added = 0;
    int n;

    if (lib == NULL || ref == NULL || out == NULL || strncmp(ref, "hvsc:", 5) != 0) return 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_DS_KEY;
    op.ref = ref;
    op.data = (uint8_t*)key;
    op.cap = sizeof key;
    if (sel_deepsid_art_ask(lib, &op) <= 0 || (slash = strrchr(key, '/')) == NULL) return 0;
    *slash = '\0'; /* shortcut: the song's own folder, so a song in a sub-folder misses its composer; upgrade with a first-two-segments rule */
    js = malloc(DS_JSON_CAP + 1u);
    if (js == NULL) return 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_GET;
    op.album = sel_art_dz_id(key);
    op.kind = "ds_composer";
    op.data = (uint8_t*)js;
    op.cap = DS_JSON_CAP;
    n = sel_deepsid_art_ask(lib, &op);
    if (n > 0) {
        js[n > (int)DS_JSON_CAP ? (int)DS_JSON_CAP : n] = '\0';
        added = sel_deepsid_composer_rows(js, out, cap);
    } else {
        sel_library_deepsid_kick(lib, ref);
    }
    free(js);
    return added;
}
