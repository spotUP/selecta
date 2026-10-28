/* View button and Amiga runtime pref engine lines (MUI-P8-5, P8-14). */
#include <selecta/uimodel/viewcmds.h>

#include <stdio.h>
#include <string.h>

static uint32_t done(int n, uint32_t cap) { return n > 0 && (uint32_t)n < cap ? (uint32_t)n : 0u; }

uint32_t sel_uim_plbtn_line(uint32_t button, const char* name, uint32_t folder, char* out, uint32_t cap) {
    if (button > 1u) {
        return 0;
    }
    if (name == NULL || name[0] == '\0') {
        name = button == 0u ? "New Folder" : "New Playlist";
    }
    return button == 0u ? done(snprintf(out, cap, "FOLDER_ADD \"%s\" %u", name, (unsigned)folder), cap)
                        : done(snprintf(out, cap, "PLAYLIST_CREATE \"%s\" %u", name, (unsigned)folder), cap);
}

const char* sel_uim_flag_label(uint32_t k) { return k == 0u ? "Mark Wrong" : k == 1u ? "Skip Song" : k == 2u ? "Save Note" : NULL; }

uint32_t sel_uim_flag_line(uint32_t k, const char* note, char* out, uint32_t cap) {
    int has = note != NULL && note[0] != '\0';
    if (k == 1u) {
        return done(snprintf(out, cap, "FLAG skip"), cap);
    }
    if (k == 0u) {
        return has ? done(snprintf(out, cap, "FLAG wrong \"%s\"", note), cap) : done(snprintf(out, cap, "FLAG wrong"), cap);
    }
    if (k == 2u) {
        return has ? done(snprintf(out, cap, "NOTE \"%s\"", note), cap) : done(snprintf(out, cap, "NOTE"), cap);
    }
    return 0;
}

int32_t sel_uim_priority_clamp(int32_t v) { return v < SEL_UIM_PRIORITY_MIN ? SEL_UIM_PRIORITY_MIN : v > SEL_UIM_PRIORITY_MAX ? SEL_UIM_PRIORITY_MAX : v; }

uint32_t sel_uim_hotkeys_line(int enabled, char* out, uint32_t cap) { return done(snprintf(out, cap, "PREF_SET amiga.hotkeys %d", enabled != 0), cap); }

uint32_t sel_uim_priority_line(int32_t v, char* out, uint32_t cap) {
    return done(snprintf(out, cap, "PREF_SET amiga.task_priority %d", (int)sel_uim_priority_clamp(v)), cap);
}

uint32_t sel_uim_groupfile_line(const char* path, char* out, uint32_t cap) {
    return done(snprintf(out, cap, "PREF_SET amiga.group_file \"%s\"", path != NULL ? path : ""), cap);
}

uint32_t sel_uim_stats_rows(const char* stats) {
    uint32_t n = 0;
    const char* p;
    for (p = stats; p != NULL && *p != '\0'; n++) {
        p += strlen(p) + 1u;
        p += strlen(p) + 1u;
    }
    return n;
}

uint32_t sel_uim_stats_row(const char* stats, uint32_t i, const char** label, const char** value) {
    const char* p = stats;
    uint32_t k;
    for (k = 0; p != NULL && *p != '\0' && k < i; k++) {
        p += strlen(p) + 1u;
        p += strlen(p) + 1u;
    }
    if (p == NULL || *p == '\0') {
        return 0;
    }
    if (label != NULL) {
        *label = p;
    }
    if (value != NULL) {
        *value = p + strlen(p) + 1u;
    }
    return 1;
}

uint32_t sel_uim_stats_shown(const char* stats, int more) {
    uint32_t n = sel_uim_stats_rows(stats);
    return more || n <= SEL_UIM_STATS_LESS ? n : SEL_UIM_STATS_LESS;
}
