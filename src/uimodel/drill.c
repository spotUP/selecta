/* The source drill rules: see include/selecta/uimodel/drill.h. Lifted from desktop_ui.c (drl_split, drl_label, the key in
 * drl_open_q) so the text UI and the desktop name the levels the same way. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <selecta/engine.h>
#include <selecta/platform.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/drill.h>

uint32_t sel_uim_drill_split(const char* path, char (*part)[SEL_UIM_DRILL_PART]) {
    uint32_t n = 0u;
    const char* p = path;
    while (*p != '\0' && n < 3u) {
        size_t len = strcspn(p, "\x1f");
        snprintf(part[n], SEL_UIM_DRILL_PART, "%.*s", (int)(len < SEL_UIM_DRILL_PART - 1u ? len : SEL_UIM_DRILL_PART - 1u), p);
        n++;
        p += len;
        if (*p == '\x1f') {
            p++;
        }
    }
    return n;
}

void sel_uim_drill_label(uint32_t page, const char* path, const char* name, char* out, size_t cap) {
    uint32_t f;
    char part[3][SEL_UIM_DRILL_PART];
    uint32_t np = sel_uim_drill_split(path, part);
    snprintf(out, cap, "%s", name);
    if (name[0] == '\0' && np > 0u && (strcmp(part[0], "artists") == 0 || (np == 2u && strcmp(part[0], "format") == 0))) { /* I911 */
        snprintf(out, cap, "Unknown artist");
        return;
    }
    sel_uim_drill_row_label(path, name, out, cap); /* I1440m/s: a DeepSID folder row shows its leaf */
    if (strcmp(out, name) != 0) {
        return;
    }
    if (page == (uint32_t)SEL_DRILL_PAGE_ROOT) {
        out[0] = (char)(out[0] >= 'a' && out[0] <= 'z' ? out[0] - 32 : out[0]);
        return;
    }
    if (np > 0u && strcmp(part[0], "platforms") == 0) {
        for (f = 0u; f < (uint32_t)SEL_PLATFORM_FAMILY_COUNT; f++) {
            if (strcmp(sel_platform_id(f), name) == 0) {
                snprintf(out, cap, "%s", sel_platform_label(f));
                return;
            }
        }
    }
}

void sel_uim_drill_key(const char* source, const char* path, uint32_t skip, const char* find, char* out, size_t cap) {
    size_t n;
    snprintf(out, cap, "%s%s%s", source, path[0] != '\0' ? "\x1f" : "", path);
    if (skip > 0u || find[0] != '\0') {
        n = strlen(out);
        snprintf(out + n, cap - n, "\x1e%u", (unsigned)skip);
    }
    if (find[0] != '\0') {
        n = strlen(out);
        snprintf(out + n, cap - n, "\x1d%s", find);
    }
}

void sel_uim_drill_up(const char* path, char* out, size_t cap) {
    char part[3][SEL_UIM_DRILL_PART];
    uint32_t np = sel_uim_drill_split(path, part);
    out[0] = '\0';
    if (np == 3u) { /* format a b -> format a */
        snprintf(out, cap, "%s\x1f%s", part[0], part[1]);
    } else if (np == 2u && strcmp(part[0], "platform") == 0) {
        snprintf(out, cap, "platforms");
    } else if (np == 2u && strcmp(part[0], "format") == 0) {
        snprintf(out, cap, "formats");
    } else if (np == 2u && strcmp(part[0], "artist") == 0) {
        snprintf(out, cap, "artists");
    }
}

int sel_uim_drill_engine_find(const char* path) {
    const char* us = strchr(path, 0x1f); /* I1442: the artist levels too ("artists", "format US name"); a songs level has a second US */
    return strcmp(path, "formats") == 0 || strcmp(path, "artists") == 0 ||
           (strncmp(path, "format", 6) == 0 && path[6] == 0x1f && strchr(us + 1, 0x1f) == NULL);
}

int sel_uim_drill_filterable(uint32_t page, const char* path, uint32_t n, uint32_t skip, const char* find) {
    if (sel_uim_drill_engine_find(path)) { /* the engine filters the whole list, so any window state takes the field; it stays while a filtered page loads */
        return (page == (uint32_t)SEL_DRILL_PAGE_GROUPS && n > 0u) || find[0] != '\0';
    }
    return page == (uint32_t)SEL_DRILL_PAGE_GROUPS && n > 0u && skip == 0u && n < (uint32_t)SEL_DRILL_WINDOW;
}

/* I1440m: the C64 SID drill kinds: the list kinds and the one-item kinds a click opens. */
static const char* drill_deepsid_kind(const char* kind, const char** root) {
    static const char* const k[8][3] = {{"tag", "tags", "Tags"}, {"group", "groups", "Groups"}, {"player", "players", "Players"}, {"folder", "collections", "Collections"},
                                        {"chart", "charts", "Charts"}, {"year", "compos", "Compos"}, {"event", "compos", "Compos"}, {"compos", "compos", "Compos"}};
    uint32_t i;
    for (i = 0u; i < 8u; i++) {
        if (strcmp(kind, k[i][0]) == 0 || strcmp(kind, k[i][1]) == 0) {
            *root = k[i][1];
            return k[i][2];
        }
    }
    return NULL;
}

uint32_t sel_uim_drill_crumbs(const char* source_name, const char* path, char (*label)[96], char (*cpath)[160]) {
    char part[3][SEL_UIM_DRILL_PART];
    uint32_t np = sel_uim_drill_split(path, part), n, k;
    const char* dsroot;
    const char* dslabel;
    snprintf(label[0], 96, "%s", source_name);
    cpath[0][0] = '\0';
    if (np == 0u) {
        return 1u;
    }
    snprintf(label[1], 96, "%s", strcmp(part[0], "format") == 0 || strcmp(part[0], "formats") == 0 ? "Formats"
                                 : strcmp(part[0], "artist") == 0 || strcmp(part[0], "artists") == 0 ? "Artists"
                                 : strcmp(part[0], "platform") == 0 || strcmp(part[0], "platforms") == 0 ? "Platforms" : "New");
    snprintf(cpath[1], 160, "%s", strcmp(part[0], "format") == 0 ? "formats" : strcmp(part[0], "artist") == 0 ? "artists" : strcmp(part[0], "platform") == 0 ? "platforms" : part[0]);
    dslabel = drill_deepsid_kind(part[0], &dsroot);
    if (dslabel != NULL) { /* I1440m */
        snprintf(label[1], 96, "%s", dslabel);
        snprintf(cpath[1], 160, "%s", dsroot);
    }
    n = 2u;
    for (k = 1u; k < np; k++) {
        char tmp[96];
        uint32_t f;
        snprintf(tmp, sizeof tmp, "%s", part[k]);
        for (f = 0u; strcmp(part[0], "platform") == 0 && k == 1u && f < (uint32_t)SEL_PLATFORM_FAMILY_COUNT; f++) {
            if (strcmp(sel_platform_id(f), part[k]) == 0) {
                snprintf(tmp, sizeof tmp, "%s", sel_platform_label(f));
            }
        }
        snprintf(label[n], 96, "%s", tmp);
        if (k == 1u) {
            snprintf(cpath[n], 160, "%s\x1f%s", part[0], part[1]);
        } else {
            snprintf(cpath[n], 160, "%s\x1f%s\x1f%s", part[0], part[1], part[2]);
        }
        n++;
    }
    return n;
}

void sel_uim_drill_key_parse(const char* key, char* source, size_t scap, char* path, size_t pcap, uint32_t* skip, char* find, size_t fcap) {
    size_t bn = strcspn(key, "\x1f\x1e");
    const char* rest = key + bn;
    const char* sk = strchr(rest, '\x1e');
    const char* fd = sk != NULL ? strchr(sk, '\x1d') : NULL;
    size_t plen = (sk != NULL ? (size_t)(sk - rest) : strlen(rest)) - (rest[0] == '\x1f' ? 1u : 0u);
    snprintf(source, scap, "%.*s", (int)bn, key);
    snprintf(path, pcap, "%.*s", (int)plen, rest + (rest[0] == '\x1f' ? 1 : 0));
    *skip = sk != NULL ? (uint32_t)strtoul(sk + 1, NULL, 10) : 0u;
    snprintf(find, fcap, "%s", fd != NULL ? fd + 1 : "");
}

void sel_uim_drill_count(uint32_t page, const char* count, char* out, size_t cap) {
    if (count[0] == '\0') {
        out[0] = '\0';
    } else {
        snprintf(out, cap, page == (uint32_t)SEL_DRILL_PAGE_ROOT ? "%s" : "%s songs", count);
    }
}
