/* I1440k: Song Information rows from the DeepSID 'ds_file' answer (design 2026-11-16-i1440-deepsid.md 3.2). Parsing lives here; the views only draw the rows. */
#include "db_internal.h"

#include "../song_key.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DS_ROW_MAX 256u
#define DS_PLACINGS_MAX 6u
#define DS_TAG_GROUPS 5u
#define DS_CSDB_MAX 6u
#define DS_YT_MAX 12u
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

/* I1465b (design D5): the objects {channel, videoId, subtune, isDefault} of the file answer's "youtube" array, one per call: *p walks the array, seg gets the object text.
 * Returns 0 when the array is missing or spent. */
static int yt_next(const char** p, const char* end, char* seg, size_t cap) {
    const char *obj, *close;
    size_t len;

    if (end == NULL || (obj = strchr(*p, '{')) == NULL || obj >= end || (close = strchr(obj, '}')) == NULL) return 0;
    len = (size_t)(close - obj) < cap - 1u ? (size_t)(close - obj) : cap - 1u;
    memcpy(seg, obj, len);
    seg[len] = '\0';
    *p = close + 1;
    return 1;
}

static const char* yt_array(const char* json) {
    const char* p = json != NULL ? strstr(json, "\"youtube\"") : NULL;

    p = p != NULL ? sel_json_value(p + 9) : NULL;
    return p != NULL && *p == '[' ? p : NULL;
}

uint32_t sel_deepsid_youtube(const char* json, int subtune, char* url, uint32_t cap) {
    char seg[256], id[48], first[48];
    const char *p = yt_array(json), *end = p != NULL ? strchr(p, ']') : NULL;

    first[0] = '\0';
    if (url == NULL || cap < 48u) return 0;
    url[0] = '\0';
    while (yt_next(&p, end, seg, sizeof seg)) {
        id[0] = '\0';
        sel_json_str(seg, "videoId", id, sizeof id);
        if (id[0] == '\0' || (subtune > 0 && num_of(seg, "subtune") != subtune)) continue;
        if (strstr(seg, "\"isDefault\":true") != NULL) {
            snprintf(url, cap, "https://www.youtube.com/watch?v=%s", id);
            return (uint32_t)strlen(url);
        }
        if (first[0] == '\0') memcpy(first, id, sizeof first);
    }
    if (first[0] == '\0') return 0;
    snprintf(url, cap, "https://www.youtube.com/watch?v=%s", first);
    return (uint32_t)strlen(url);
}

/* D5b: every capture as a "YouTube capture" row, "Subtune N, <channel>: <url>" (max DS_YT_MAX). */
static void youtube_rows(const char* json, char* out, uint32_t cap, uint32_t* added) {
    char seg[256], id[48], ch[64], row[DS_ROW_MAX];
    const char *p = yt_array(json), *end = p != NULL ? strchr(p, ']') : NULL;
    uint32_t n = 0;

    while (n < DS_YT_MAX && yt_next(&p, end, seg, sizeof seg)) {
        id[0] = ch[0] = '\0';
        sel_json_str(seg, "videoId", id, sizeof id);
        sel_json_str(seg, "channel", ch, sizeof ch);
        if (id[0] == '\0') continue;
        snprintf(row, sizeof row, "Subtune %d, %s: https://www.youtube.com/watch?v=%s", num_of(seg, "subtune"), ch[0] != '\0' ? ch : "unknown channel", id);
        *added += (uint32_t)put(out, cap, "YouTube capture", row);
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
    youtube_rows(json, out, cap, &added);
    a[0] = '\0';
    sel_json_str(json, "gb64", a, sizeof a); /* a JSON null never matches: the scanner wants a quote */
    added += (uint32_t)put(out, cap, "GB64 game", a);
    if (rows != NULL) *rows += added;
    return added;
}

/* I1440l: "CSDb release" rows from a ds_csdb answer {csdbId, releases[{id,name,screenshot[,group,year,type]}]}. The server sends id, name and screenshot today; group, year and type are shown when present. */
uint32_t sel_deepsid_csdb_rows(const char* json, char* out, uint32_t cap, uint32_t* rows) {
    const char *p, *end, *rel, *next;
    char seg[512], name[96], grp[64], type[32], row[DS_ROW_MAX];
    uint32_t n = 0, added = 0;
    size_t len;
    int id, year;

    if (json == NULL || out == NULL) return 0;
    p = strstr(json, "\"releases\"");
    p = p != NULL ? sel_json_value(p + 10) : NULL;
    end = p != NULL && *p == '[' ? strrchr(p, ']') : NULL;
    while (end != NULL && n < DS_CSDB_MAX && (rel = strstr(p, "\"id\"")) != NULL && rel < end) {
        next = strstr(rel + 4, "\"id\"");
        len = next != NULL && next < end ? (size_t)(next - rel) : (size_t)(end - rel);
        if (len >= sizeof seg) len = sizeof seg - 1u;
        memcpy(seg, rel, len);
        seg[len] = '\0';
        p = rel + 4;
        id = num_of(seg, "id");
        year = num_of(seg, "year");
        name[0] = grp[0] = type[0] = '\0';
        sel_json_str(seg, "name", name, sizeof name);
        sel_json_str(seg, "group", grp, sizeof grp);
        sel_json_str(seg, "type", type, sizeof type);
        if (id <= 0 || name[0] == '\0') continue;
        snprintf(row, sizeof row, "%s", name);
        if (grp[0] != '\0') snprintf(row + strlen(row), sizeof row - strlen(row), ", %s", grp);
        if (year > 0) snprintf(row + strlen(row), sizeof row - strlen(row), ", %d", year);
        if (type[0] != '\0') snprintf(row + strlen(row), sizeof row - strlen(row), ", %s", type);
        snprintf(row + strlen(row), sizeof row - strlen(row), " (csdb.dk/release/?id=%d)", id); /* the panel has no open-URL action or image slot: the URL is shown as text */
        added += (uint32_t)put(out, cap, "CSDb release", row);
        n++;
    }
    if (rows != NULL) *rows += added;
    return added;
}

/* I1440l2: the release ids (max 6) of a ds_csdb answer, for the thumbnail fetcher. */
uint32_t sel_deepsid_csdb_ids(const char* json, int* ids, uint32_t max) {
    const char* p = json != NULL ? strstr(json, "\"releases\"") : NULL;
    uint32_t n = 0;

    while (p != NULL && n < max && n < DS_CSDB_MAX && (p = strstr(p + 1, "\"id\"")) != NULL) {
        const char* v = sel_json_value(p + 4);
        int id = v != NULL ? atoi(v) : 0;

        if (id > 0) ids[n++] = id;
    }
    return n;
}

/* Lowercase letters and digits only, so "Last Ninja!" equals "last ninja". */
static void norm(const char* in, char* out, size_t cap) {
    size_t n = 0;

    for (; *in != '\0' && n + 1 < cap; in++) {
        char c = *in >= 'A' && *in <= 'Z' ? (char)(*in + 32) : *in;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[n++] = c;
    }
    out[n] = '\0';
}

/* The game named by a STIL "[from the C64 game X]" line, normalised into out ("" when there is none). */
static void stil_game(const char* stil, char* out, size_t cap) {
    char game[160];
    const char* g = strstr(stil, "from the C64 game ");
    size_t m = 0;

    out[0] = '\0';
    if (g == NULL) return;
    for (g += 18; *g != '\0' && *g != ']' && *g != '\n' && m + 1 < sizeof game; g++) game[m++] = *g;
    game[m] = '\0';
    norm(game, out, cap);
}

uint32_t sel_deepsid_csdb_own(const char* file_json, const char* csdb_json, int* ids, uint32_t max) {
    char type[16], copy[160], title[160], stil[1024], want[2][160], grp[64], name[160], nn[160];
    const char *p, *end, *rel, *next;
    char seg[512];
    uint32_t n = 0;
    size_t len;
    int cy, id, year, k;

    if (file_json == NULL || ids == NULL || max == 0) return 0;
    type[0] = '\0';
    sel_json_str(file_json, "csdbType", type, sizeof type);
    if (strcmp(type, "release") == 0) {
        id = num_of(file_json, "csdbId");
        if (id > 0) ids[n++] = id;
        return n;
    }
    if (strcmp(type, "sid") != 0 || csdb_json == NULL) return 0;
    copy[0] = title[0] = stil[0] = '\0';
    sel_json_str(file_json, "copyright", copy, sizeof copy);
    sel_json_str(file_json, "title", title, sizeof title); /* shortcut: the answer's song-name key is "title"; unverified against the live server */
    sel_json_str(file_json, "stil", stil, sizeof stil);
    cy = atoi(copy);
    norm(title, want[0], sizeof want[0]);
    want[1][0] = '\0';
    stil_game(stil, want[1], sizeof want[1]);
    p = strstr(csdb_json, "\"releases\"");
    p = p != NULL ? sel_json_value(p + 10) : NULL;
    end = p != NULL && *p == '[' ? strrchr(p, ']') : NULL;
    while (end != NULL && n < max && (rel = strstr(p, "\"id\"")) != NULL && rel < end) {
        next = strstr(rel + 4, "\"id\"");
        len = next != NULL && next < end ? (size_t)(next - rel) : (size_t)(end - rel);
        if (len >= sizeof seg) len = sizeof seg - 1u;
        memcpy(seg, rel, len);
        seg[len] = '\0';
        p = rel + 4;
        id = num_of(seg, "id");
        year = num_of(seg, "year");
        grp[0] = name[0] = '\0';
        sel_json_str(seg, "group", grp, sizeof grp);
        sel_json_str(seg, "name", name, sizeof name);
        norm(name, nn, sizeof nn);
        if (id <= 0 || cy <= 0 || year != cy) continue;
        for (k = 0; grp[k] != '\0'; k++) grp[k] = grp[k] >= 'A' && grp[k] <= 'Z' ? (char)(grp[k] + 32) : grp[k]; /* ci_has wants a lowercase needle */
        k = (grp[0] != '\0' && ci_has(copy, grp)) || (nn[0] != '\0' && (strcmp(nn, want[0]) == 0 || strcmp(nn, want[1]) == 0));
        if (k) ids[n++] = id;
    }
    return n;
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
        op.kind = "ds_csdb"; /* written by the same kick as ds_file, so no second kick */
        n = sel_deepsid_art_ask(lib, &op);
        if (n > 0) {
            js[n > (int)DS_JSON_CAP ? (int)DS_JSON_CAP : n] = '\0';
            added += sel_deepsid_csdb_rows(js, out, cap, rows);
        }
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

/* I1465f (design D3a): the `gb64` field of a DeepSID file answer is a PHP-serialized list of related games,
 * a:N:{i:0;a:2:{s:4:"name";s:10:"Uridium v2";s:2:"id";s:5:"11606";}...}. A bounded count `lead:<digits><end>` at p; NULL when malformed. */
static const char* php_num(const char* p, char lead, char end, long* out) {
    long v = 0;
    int d = 0;

    if (p[0] != lead || p[1] != ':') return NULL;
    for (p += 2; *p >= '0' && *p <= '9' && d < 9; p++, d++) v = v * 10 + (*p - '0');
    if (d == 0 || *p != end) return NULL;
    *out = v;
    return p + 1;
}

/* s:<len>:"<bytes>"; or i:<n>; at p as text into out (cut to cap); returns the position after it, NULL when neither or the length runs past the NUL. */
static const char* php_val(const char* p, char* out, size_t cap) {
    long n;
    size_t i;
    const char* q;

    if (p[0] == 'i') {
        q = php_num(p, 'i', ';', &n);
        if (q == NULL) return NULL;
        snprintf(out, cap, "%ld", n);
        return q;
    }
    q = php_num(p, 's', ':', &n);
    if (q == NULL || q[0] != '"') return NULL;
    q++;
    for (i = 0; i < (size_t)n; i++) {
        if (q[i] == '\0') return NULL;
    }
    if (q[n] != '"' || q[n + 1] != ';') return NULL;
    i = (size_t)n < cap - 1u ? (size_t)n : cap - 1u;
    memcpy(out, q, i);
    out[i] = '\0';
    return q + n + 2;
}

uint32_t sel_deepsid_gb64_parse(const char* php, sel_gb64_entry* out, uint32_t max) {
    const char* p;
    long cnt, k, idx, fields, f;
    uint32_t n = 0;

    if (php == NULL || out == NULL || max == 0) return 0;
    p = php_num(php, 'a', ':', &cnt);
    if (p == NULL || *p++ != '{') return 0;
    for (k = 0; k < cnt && n < max; k++) {
        sel_gb64_entry e;

        memset(&e, 0, sizeof e);
        p = php_num(p, 'i', ';', &idx);
        p = p != NULL ? php_num(p, 'a', ':', &fields) : NULL;
        if (p == NULL || *p++ != '{') break;
        for (f = 0; f < fields; f++) {
            char key[16], val[sizeof e.name];

            p = php_val(p, key, sizeof key);
            p = p != NULL ? php_val(p, val, sizeof val) : NULL;
            if (p == NULL) break;
            if (strcmp(key, "name") == 0) memcpy(e.name, val, sizeof e.name);
            else if (strcmp(key, "id") == 0) e.id = atoi(val);
        }
        if (f < fields || *p++ != '}') break; /* a malformed entry ends the list; the ones before it stay */
        if (e.id > 0) out[n++] = e;
    }
    return n;
}

/* 1 when the normalised `have` matches `want` at the tier: 0 equal, 1 one starts with the other, 2 one contains the other. */
static int gb64_tier(const char* have, const char* want, int tier) {
    size_t h = strlen(have), w = strlen(want);

    if (h == 0 || w == 0) return 0;
    if (tier == 0) return strcmp(have, want) == 0;
    if (tier == 1) return strncmp(have, want, h < w ? h : w) == 0;
    return strstr(have, want) != NULL || strstr(want, have) != NULL;
}

int sel_deepsid_gb64_pick(const sel_gb64_entry* e, uint32_t n, const char* title) {
    char want[160], have[160];
    uint32_t i;
    int tier;

    if (e == NULL || n == 0) return 0;
    norm(title != NULL ? title : "", want, sizeof want);
    for (tier = 0; tier < 3; tier++) {
        for (i = 0; i < n; i++) {
            norm(e[i].name, have, sizeof have);
            if (gb64_tier(have, want, tier)) return e[i].id;
        }
    }
    return e[0].id;
}

int sel_deepsid_gb64_id(const char* json) {
    char raw[2048], title[160], stil[1024], app[24];
    sel_gb64_entry e[8];
    const char *p, *end;
    size_t n = 0;
    int game = 0;

    if (json == NULL) return 0;
    app[0] = '\0';
    sel_json_str(json, "application", app, sizeof app);
    p = strstr(json, "\"tags\"");
    p = p != NULL ? sel_json_value(p + 6) : NULL;
    end = p != NULL && *p == '[' ? strchr(p, ']') : NULL;
    for (p = end != NULL ? strstr(p, "\"Game\"") : NULL; p != NULL && p < end; p = strstr(p + 6, "\"Game\"")) game = 1;
    if (!game || strcmp(app, "RELEASE") != 0) return 0;
    p = strstr(json, "\"gb64\"");
    p = p != NULL ? sel_json_value(p + 6) : NULL;
    if (p == NULL || *p != '"') return 0; /* null or absent */
    for (p++; *p != '\0' && *p != '"' && n + 1 < sizeof raw; p++) {
        if (*p == '\\' && p[1] != '\0') p++; /* the serialized text sits in a JSON string: \" \\ \/ unescape to the character */
        raw[n++] = *p;
    }
    raw[n] = '\0';
    title[0] = stil[0] = '\0';
    sel_json_str(json, "title", title, sizeof title);
    sel_json_str(json, "stil", stil, sizeof stil);
    stil_game(stil, stil, sizeof stil); /* in place: the normalised text is never longer */
    return sel_deepsid_gb64_pick(e, sel_deepsid_gb64_parse(raw, e, 8), stil[0] != '\0' ? stil : title);
}
