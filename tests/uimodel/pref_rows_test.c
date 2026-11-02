/* MPS-1b: the Settings rows each machine shows (src/uimodel/features.c). Host SID rows are dead knobs on the Amiga; playsid.* and
 * ui.desktop.overscan do nothing on the host. Both Settings UIs read this one filtered list. */
#include <selecta/uimodel.h>
#include <stdio.h>
#include <string.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int has(uint32_t m, const char* key) {
    uint32_t i;
    for (i = 0; i < sel_uim_pref_row_count_for(m); i++) {
        if (strcmp(sel_uim_pref_row_at_for(m, i)->key, key) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    static const char* const host_only[] = {"sid.model", "sid.clock", "sid.filter", "sid.digiboost"};
    static const char* const amiga_only[] = {"playsid.filter", "playsid.boost", "playsid.oversample", "ui.desktop.overscan"};
    uint32_t i;
    for (i = 0; i < 4; i++) {
        CHECK(has(SEL_UIM_MACH_HOST, host_only[i]) && !has(SEL_UIM_MACH_AMIGA, host_only[i]));
        CHECK(!has(SEL_UIM_MACH_HOST, amiga_only[i]) && has(SEL_UIM_MACH_AMIGA, amiga_only[i]));
    }
    CHECK(has(SEL_UIM_MACH_HOST, "mix.loudness") && has(SEL_UIM_MACH_AMIGA, "mix.loudness"));
    CHECK(has(SEL_UIM_MACH_HOST, "view.status_format") && has(SEL_UIM_MACH_AMIGA, "view.status_format"));
    CHECK(sel_uim_pref_row_count_for(SEL_UIM_MACH_HOST) == 12u && sel_uim_pref_row_count_for(SEL_UIM_MACH_AMIGA) == 12u);
    CHECK(sel_uim_pref_row_count_for(SEL_UIM_MACH_ALL) == 16u && sel_uim_pref_row_count_for(SEL_UIM_MACH_ALL) <= SEL_UIM_PREF_ROWS_MAX);
    CHECK(sel_uim_pref_row_at_for(SEL_UIM_MACH_HOST, 12u) == NULL);
    /* The unsuffixed iterator is the one both UIs call: it is the filtered list of this build's machine. */
    CHECK(sel_uim_pref_row_count() == sel_uim_pref_row_count_for(sel_uim_machine_here()));
    CHECK(sel_uim_pref_row_at(0) == sel_uim_pref_row_at_for(sel_uim_machine_here(), 0));
    if (g_fail == 0) {
        printf("pref-rows: ok\n");
    }
    return g_fail != 0;
}
