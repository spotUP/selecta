/* I774n R8: the song info sections Credits, Other Sources and Found In (src/uimodel/siinfo.c), content only. The Mac, text and MUI UIs
 * render these lines (R9-R11); none of them re-derives a value. Same Tune (needs uadegroups.bin, R12/R13) and Demozoo (needs the
 * hash join, R15) produce no section yet: with no data there is no heading and no line. Pure C99.
 * Design: thoughts/shared/designs/2026-11-17-i774-uade-tools-all.md section 5.1.
 * Call order: sel_uadesong_load (src/library/uadedb.h, fills sel_uim_si_input from the bake files beside library.db), then
 * sel_uim_si_lines. */
#ifndef SELECTA_UIMODEL_SIINFO_H
#define SELECTA_UIMODEL_SIINFO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SEL_UIM_SI_CREDIT_MAX 9u
#define SEL_UIM_SI_PROV_MAX 64u
/* One credit row: the distilled catalogue metadata (info) or a per-source row. Empty string / 0 = unknown. */
typedef struct {
    char source[16];
    char format[64];
    char authors[256]; /* "~"-joined upstream list */
    char publisher[128];
    char album[128];
    uint32_t channels, year;
} sel_uim_si_credit;
typedef struct sel_uim_si_input {
    int have_info;
    sel_uim_si_credit info;
    uint32_t ncredits;
    sel_uim_si_credit credits[SEL_UIM_SI_CREDIT_MAX];
    uint32_t nprov;
    struct {
        int kind; /* 0 site, 1 collection, 2 group, 3 BBS, 4 CD-ROM (the order of SEL_UPRV_* in src/library/uadedb.h) */
        char display[64];
    } prov[SEL_UIM_SI_PROV_MAX];
} sel_uim_si_input;

typedef enum {
    SEL_UIM_SI_HEADING,    /* key: the section heading ("Credits", "Other Sources", "Found In") */
    SEL_UIM_SI_CREDIT,     /* key: label (Authors, Group / Publisher, Release, Year, Format, Channels), val: the value */
    SEL_UIM_SI_OTHER,      /* key: the source ("AMP"), val: the fields that source gives, ", " joined */
    SEL_UIM_SI_FOUND_KIND, /* key: kind label ("Sites"), val: the count; the SEL_UIM_SI_FOUND_NAME lines of that kind follow it */
    SEL_UIM_SI_FOUND_NAME  /* val: one source name; shown on expand */
} sel_uim_si_kind;
typedef struct {
    sel_uim_si_kind kind;
    char key[32];
    char val[384];
} sel_uim_si_line;
#define SEL_UIM_SI_LINES_MAX 96u

/* Builds the lines in display order; a section with no data is left out, including its heading. Returns the count (<= max). */
uint32_t sel_uim_si_lines(const sel_uim_si_input* in, sel_uim_si_line* out, uint32_t max);

/* I774n R9: the lines as "label\0value\0" panel pairs (sel_state.panel), appended at out[*used] (the list ends in an empty label, as
 * panel_put in src/library/db/user.c leaves it). The line kind rides in the first label byte: heading 0x01, Found In kind 0x02,
 * Found In name 0x03 (empty label); Credits and Other Sources rows are plain pairs. Stops when a pair does not fit. Returns the pairs added. */
uint32_t sel_uim_si_panel_put(const sel_uim_si_input* in, char* out, uint32_t cap, uint32_t* used);
/* Decodes a panel label: returns its kind (CREDIT for a plain pair) and sets *text to the label without the tag byte. */
sel_uim_si_kind sel_uim_si_panel_kind(const char* label, const char** text);
/* I774n R11b: index (0..SEL_UIM_SI_FOUND_N-1) of a Found In kind label, -1 when unknown; the UI keeps one expand bit per index. */
#define SEL_UIM_SI_FOUND_N 5
int sel_uim_si_found_index(const char* text);
/* I774n R11c: the five section headings in display order (Credits, Other Sources, Found In, Same Tune, Demozoo); one list per index in the MUI. */
#define SEL_UIM_SI_SECTION_N 5
extern const char* const sel_uim_si_sections[SEL_UIM_SI_SECTION_N];
int sel_uim_si_section_index(const char* heading); /* -1 when unknown */

#ifdef __cplusplus
}
#endif
#endif
