/* dui-font-roles (I526-P4): the role table of the bitmap tier. Every size token of DESK_FONT_SIZES in both weights maps
 * to a baked atlas of the expected family, and a box shorter than the line takes the family's 6-row font. */
#include "font_roles.h"

#include <stdio.h>

static int bad;

static void expect(int cond, const char* what, int a, int b) {
    if (!cond) {
        fprintf(stderr, "font_roles_unit: FAIL %s (%d %d)\n", what, a, b);
        bad++;
    }
}

typedef struct {
    int semibold, size, family, px;
} role_case;

int main(void) {
    static const int sizes[] = {DESK_FONT_SIZES};
    static const role_case roles[] = {
        {1, 32, DESK_FAMILY_INTER_SEMIBOLD, 24}, {1, 24, DESK_FAMILY_INTER_SEMIBOLD, 18}, {1, 22, DESK_FAMILY_INTER_SEMIBOLD, 18},
        {1, 18, DESK_FAMILY_INTER_SEMIBOLD, 18}, {1, 16, DESK_FAMILY_MINI8, 8},           {0, 15, DESK_FAMILY_MINI8, 8},
        {1, 14, DESK_FAMILY_MINI8, 8},           {1, 13, DESK_FAMILY_MINI8, 8},           {0, 13, DESK_FAMILY_MINI6, 8},
        {0, 12, DESK_FAMILY_MINI6, 8},           {1, 12, DESK_FAMILY_MINI6, 8},           {0, 11, DESK_FAMILY_MINI4, 8},
        {1, 11, DESK_FAMILY_MINI4, 8},           {0, 18, DESK_FAMILY_INTER_SEMIBOLD, 18}, {0, 32, DESK_FAMILY_INTER_SEMIBOLD, 24},
    };
    unsigned i;
    int w, k;
    for (w = 0; w < 2; w++) {
        for (k = 0; k < DESK_FONT_SIZE_N; k++) {
            expect(desk_font_role_atlas(w, sizes[k], 0, NULL, NULL) != NULL, "token without a family", w, sizes[k]);
        }
    }
    for (i = 0; i < sizeof roles / sizeof roles[0]; i++) {
        int fam = -1, px = -1;
        const sel_gui_atlas* a = desk_font_role_atlas(roles[i].semibold, roles[i].size, 0, &fam, &px);
        expect(a != NULL && fam == roles[i].family && px == roles[i].px, "role family", roles[i].semibold, roles[i].size);
    }
    {
        int fam = -1, px = -1;
        const sel_gui_atlas* a = desk_font_role_atlas(1, 13, 12, &fam, &px); /* a 6-row box, mini8 -> mini6 6x6 */
        expect(a != NULL && fam == DESK_FAMILY_MINI6 && px == 6 && a->line_height == 12, "mini8 6-row fallback", fam, px);
        a = desk_font_role_atlas(0, 11, 12, &fam, &px); /* mini4 8 rows -> mini4 4x6 */
        expect(a != NULL && fam == DESK_FAMILY_MINI4 && px == 6 && a->line_height == 12, "mini4 6-row fallback", fam, px);
        a = desk_font_role_atlas(0, 12, 16, &fam, &px); /* fits: stays at 8 rows */
        expect(a != NULL && fam == DESK_FAMILY_MINI6 && px == 8, "fitting box keeps the role", fam, px);
        a = desk_font_role_atlas(1, 32, 24, &fam, &px); /* H12 line is taller than 24: H9 */
        expect(a != NULL && fam == DESK_FAMILY_INTER_SEMIBOLD && px == 18, "headline steps down", fam, px);
    }
    if (bad == 0) {
        printf("font_roles_unit: ok\n");
    }
    return bad == 0 ? 0 : 1;
}
