#include <selecta/uimodel/siinfo.h>

/* I774n R9: stateless panel-label decode, kept in the UI model so UI plugins (also the Amiga segments) resolve it without the library;
 * the line builder and panel_put live in src/library/siinfo_lines.c (the library calls them; uimodel -> core -> library would cycle). */
sel_uim_si_kind sel_uim_si_panel_kind(const char* label, const char** text) {
    unsigned char c = (unsigned char)label[0];
    sel_uim_si_kind k = c == 1u ? SEL_UIM_SI_HEADING : c == 2u ? SEL_UIM_SI_FOUND_KIND : c == 3u ? SEL_UIM_SI_FOUND_NAME : SEL_UIM_SI_CREDIT;
    *text = c >= 1u && c <= 3u ? label + 1 : label;
    return k;
}
