/* The source drill rules: see include/selecta/uimodel/drill.h. Lifted from desktop_ui.c (drl_split, drl_child, drl_label, the key in
 * drl_open_q) so the text UI and the desktop name the levels the same way. */
#include <stdio.h>
#include <string.h>
#include <selecta/engine.h>
#include <selecta/platform.h>
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

int sel_uim_drill_child(uint32_t page, const char* path, const char* name, char* out, size_t cap) {
    char part[3][SEL_UIM_DRILL_PART];
    uint32_t np = sel_uim_drill_split(path, part);
    out[0] = '\0';
    if (page == (uint32_t)SEL_DRILL_PAGE_ROOT) {
        snprintf(out, cap, "%s", name);
    } else if (np == 1u && strcmp(part[0], "formats") == 0) {
        snprintf(out, cap, "format\x1f%s", name);
    } else if (np == 1u && strcmp(part[0], "artists") == 0) {
        snprintf(out, cap, "artist\x1f%s", name);
    } else if (np == 1u && strcmp(part[0], "platforms") == 0) {
        snprintf(out, cap, "platform\x1f%s", name);
    } else if (np == 2u && strcmp(part[0], "platform") == 0) {
        snprintf(out, cap, "format\x1f%s", name);
    } else if (np == 2u && strcmp(part[0], "format") == 0) {
        snprintf(out, cap, "format\x1f%s\x1f%s", part[1], name);
    }
    return out[0] != '\0';
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
