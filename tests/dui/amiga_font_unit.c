/* dui-amiga-font unit (I526-P1): the Amiga bitmap .font reader against hand-decoded CharData rows. The expected rows
 * below were decoded independently (a separate script reading the file with tf_Modulo as the row stride and the
 * CharLoc bit offsets), not by the reader. Also the metrics the plan fixes: glyph_n from tf_LoChar/tf_HiChar (224),
 * adv = tf_XSize, rows = tf_YSize, line_height = 2 x tf_YSize (ydiv 2). */
#include "atlas_build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AMIGA_MINI_DIR
#error "AMIGA_MINI_DIR"
#endif

typedef struct font_case {
    const char* file;
    int ysize, xsize, baseline;
    int a_w, e4_w;
    unsigned a_rows[8], e4_rows[8];
} font_case;

static const font_case cases[] = {
    {"mini8/8", 8, 8, 6, 8, 8, {24, 36, 66, 66, 126, 66, 66, 0}, {66, 0, 60, 2, 62, 66, 62, 0}},
    {"mini6/8", 8, 6, 6, 6, 6, {8, 20, 34, 34, 62, 34, 34, 0}, {20, 0, 28, 2, 30, 34, 30, 0}},
    {"mini4/6", 6, 4, 4, 4, 4, {4, 10, 14, 10, 10, 0}, {10, 14, 6, 10, 6, 0}},
};

static unsigned char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    unsigned char* b;
    long len;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (unsigned char*)malloc((size_t)len);
    if (b != NULL && fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b);
        b = NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static const sel_gui_glyph* find(const sel_gui_atlas* at, uint32_t cp) {
    uint32_t i;
    for (i = 0; i < at->glyph_n; i++) {
        if (at->glyphs[i].cp == cp) {
            return &at->glyphs[i];
        }
    }
    return NULL;
}

/* 0 when the glyph's ink equals the expected rows (MSB first over w columns). */
static int rows_differ(const sel_gui_atlas* at, const sel_gui_glyph* g, int w, int h, const unsigned* rows) {
    int r, x;
    if (g == NULL || g->w != w || g->h != h) {
        return 1;
    }
    for (r = 0; r < h; r++) {
        for (x = 0; x < w; x++) {
            int want = (int)((rows[r] >> (w - 1 - x)) & 1u);
            int got = at->alpha[(size_t)(g->y + r) * (size_t)at->page_w + (size_t)(g->x + x)] == 255u;
            if (want != got) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void) {
    size_t c;
    int bad = 0;
    for (c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        const font_case* fc = &cases[c];
        char path[512];
        size_t n = 0;
        unsigned char* data;
        sel_gui_atlas* at;
        const sel_gui_glyph *ga, *ge;
        uint32_t i;
        snprintf(path, sizeof path, "%s/%s", AMIGA_MINI_DIR, fc->file);
        data = slurp(path, &n);
        if (data == NULL || desk_amiga_font_ysize(data, n) != fc->ysize) {
            fprintf(stderr, "amiga_font_unit: %s: ysize %d, want %d\n", fc->file, data == NULL ? -1 : desk_amiga_font_ysize(data, n), fc->ysize);
            bad++;
            continue;
        }
        at = desk_atlas_build_amiga(data, n);
        if (at == NULL) {
            fprintf(stderr, "amiga_font_unit: %s: build failed\n", fc->file);
            bad++;
            continue;
        }
        if (at->glyph_n != 224u + 7u /* Latin-1 plus 7 punctuation aliases (I526 P5) */ || at->line_height != 2 * fc->ysize || at->ascent != (fc->baseline + 1) * 2 ||
            at->missing.cp != 0xFFFFFFFFu) {
            fprintf(stderr, "amiga_font_unit: %s: glyph_n=%u line_height=%d ascent=%d\n", fc->file, (unsigned)at->glyph_n, at->line_height, at->ascent);
            bad++;
        }
        for (i = 0; i < at->glyph_n; i++) {
            if (at->glyphs[i].adv != fc->xsize || (at->glyphs[i].w != 0 && at->glyphs[i].h != fc->ysize)) {
                fprintf(stderr, "amiga_font_unit: %s: glyph %u adv=%d h=%d, want adv=%d h=%d\n", fc->file, (unsigned)at->glyphs[i].cp,
                        at->glyphs[i].adv, at->glyphs[i].h, fc->xsize, fc->ysize);
                bad++;
                break;
            }
        }
        if (find(at, 0x2013) == NULL || find(at, 0x2013)->x != find(at, '-')->x || find(at, 0x2022) == NULL ||
            find(at, 0x2022)->x != find(at, 0xB7)->x) { /* I526 P5: the en dash and the bullet are not missing glyphs */
            fprintf(stderr, "amiga_font_unit: %s: en dash / bullet alias missing\n", fc->file);
            bad++;
        }
        ga = find(at, 'A');
        ge = find(at, 0xE4);
        if (rows_differ(at, ga, fc->a_w, fc->ysize, fc->a_rows)) {
            fprintf(stderr, "amiga_font_unit: %s: 'A' differs from the hand-decoded rows\n", fc->file);
            bad++;
        }
        if (rows_differ(at, ge, fc->e4_w, fc->ysize, fc->e4_rows)) {
            fprintf(stderr, "amiga_font_unit: %s: 0xE4 differs from the hand-decoded rows\n", fc->file);
            bad++;
        }
        free((void*)at->glyphs);
        free((void*)at->kerns);
        free((void*)at->alpha);
        free(at);
        free(data);
    }
    if (desk_amiga_font_ysize((const unsigned char*)"not a font at all, not a font at all, not a font", 48) != 0) {
        fprintf(stderr, "amiga_font_unit: a non-font blob was accepted\n");
        bad++;
    }
    printf("amiga_font_unit fonts=%d bad=%d\n", (int)(sizeof cases / sizeof cases[0]), bad);
    return bad != 0;
}
