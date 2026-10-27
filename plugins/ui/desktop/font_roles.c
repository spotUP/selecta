/* I526: the font role table of the bitmap tier; see font_roles.h and thoughts/shared/plans/2026-10-27-aga-native-fonts.md
 * section 1. */
#include "font_roles.h"

typedef struct {
    int family;
    int px; /* the baked size: tf_YSize rows for the bitmap fonts, layout units for the headlines */
} role_font;

/* The fallback chain, largest first. A role starts at its entry and moves down while the box is shorter than the line. */
static const role_font k_chain[] = {
    {DESK_FAMILY_INTER_SEMIBOLD, 32}, {DESK_FAMILY_INTER_SEMIBOLD, 22}, {DESK_FAMILY_MINI8, 8},
    {DESK_FAMILY_MINI6, 8},           {DESK_FAMILY_MINI4, 8},           {DESK_FAMILY_MINI6, 6},
    {DESK_FAMILY_MINI4, 6},
};
#define CHAIN_N (int)(sizeof k_chain / sizeof k_chain[0])
#define CH_H16 0
#define CH_H11 1
#define CH_MINI8 2
#define CH_MINI6 3
#define CH_MINI4 4

/* The role of a token: headlines for 18 and up (22 for 18-24, 32 above), mini8 for 14-16 and the semibold 13, mini6 for
 * 12 and the regular 13, mini4 for 11. */
static int role_start(int semibold, int size) {
    if (size >= 25) {
        return CH_H16;
    }
    if (size >= 18) {
        return CH_H11;
    }
    if (size >= 14 || (size == 13 && semibold)) {
        return CH_MINI8;
    }
    if (size >= 12) {
        return CH_MINI6;
    }
    return CH_MINI4;
}

/* The next smaller font of the chain: a 6-row font (mini6 8 rows and mini8 go to mini6 6 rows, mini4 8 rows to mini4 6). */
static int next_smaller(int at) {
    if (at == CH_MINI4) {
        return CHAIN_N - 1; /* mini4 6 rows */
    }
    if (at >= CH_MINI8 && at < CHAIN_N - 2) {
        return CHAIN_N - 2; /* mini6 6 rows */
    }
    if (at < CH_MINI8) {
        return at + 1;
    }
    return -1;
}

const sel_gui_atlas* desk_font_role_atlas(int semibold, int size, int32_t box_h, int* family, int* px) {
    int at = role_start(semibold, size);
    const sel_gui_atlas* a = desk_atlas_baked(k_chain[at].family, k_chain[at].px, 2);
    while (a != NULL && box_h > 0 && a->line_height > box_h) {
        int nx = next_smaller(at);
        const sel_gui_atlas* b;
        if (nx < 0) {
            break;
        }
        b = desk_atlas_baked(k_chain[nx].family, k_chain[nx].px, 2);
        if (b == NULL) {
            break;
        }
        at = nx;
        a = b;
    }
    if (a != NULL && family != NULL) {
        *family = k_chain[at].family;
    }
    if (a != NULL && px != NULL) {
        *px = k_chain[at].px;
    }
    return a;
}
