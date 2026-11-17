#include <string.h>
#include <selecta/uimodel/siinfo.h>

/* I774n R9: stateless panel-label decode, kept in the UI model so UI plugins (also the Amiga segments) resolve it without the library;
 * the line builder and panel_put live in src/library/siinfo_lines.c (the library calls them; uimodel -> core -> library would cycle). */
sel_uim_si_kind sel_uim_si_panel_kind(const char* label, const char** text) {
    unsigned char c = (unsigned char)label[0];
    sel_uim_si_kind k = c == 1u ? SEL_UIM_SI_HEADING : c == 2u ? SEL_UIM_SI_FOUND_KIND : c == 3u ? SEL_UIM_SI_FOUND_NAME : SEL_UIM_SI_CREDIT;
    *text = c >= 1u && c <= 3u ? label + 1 : label;
    return k;
}

const char* const sel_uim_si_sections[SEL_UIM_SI_SECTION_N] = {"Credits", "Other Sources", "Found In", "Same Tune", "Demozoo"};
int sel_uim_si_section_index(const char* heading) {
    int k;
    for (k = 0; k < SEL_UIM_SI_SECTION_N; k++) {
        if (strcmp(heading, sel_uim_si_sections[k]) == 0) {
            return k;
        }
    }
    return -1;
}

/* I774n R11b: the Found In kind labels in builder order (src/library/siinfo_lines.c); one expand bit per entry in every UI. */
int sel_uim_si_found_index(const char* text) {
    static const char* const kinds[SEL_UIM_SI_FOUND_N] = {"Sites", "Collections", "Groups", "Bulletin Boards", "CD-ROMs"};
    int k;
    for (k = 0; k < SEL_UIM_SI_FOUND_N; k++) {
        if (strcmp(text, kinds[k]) == 0) {
            return k;
        }
    }
    return -1;
}
