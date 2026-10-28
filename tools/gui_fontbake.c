/* Host tool (I463, I526): bakes the Inter text atlases of the desktop UI into C tables, so the 68020 tier never runs
 * stb_truetype (soft float, 43.8 s for the first frame on the Replay). It calls the plugin's own desk_atlas_build, so a
 * baked atlas is the runtime atlas byte for byte. Floating point is allowed here, outside src/gui.
 *
 *   gui_fontbake <Inter-Regular.ttf> <Inter-SemiBold.ttf> <out.c> [<family>:<amiga font file> ...]
 *
 * Inter, ydiv 1: every weight at every size token of atlas_build.h at every scale of DESK_FONT_BAKE_SCALES_Q16,
 * deduplicated by pixel size. Inter, ydiv 2: SemiBold only, at DESK_FONT_BAKE_TALL_PX. Amiga bitmap fonts (family
 * mini4, mini6 or mini8), ydiv 2, size tf_YSize. */
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcomment"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wnewline-eof"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "atlas_build.h"

#include <selecta/gui/gui_icons.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* atlas_build.c looks baked atlases up; the tool has none (it makes them). */
const desk_baked_font sel_font_baked[1] = {{0, 0, 1, {0}}};
const uint32_t sel_font_baked_n = 0;

static unsigned char* slurp(const char* path) {
    FILE* f = fopen(path, "rb");
    unsigned char* b;
    long n;
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (unsigned char*)malloc((size_t)n);
    if (b != NULL && fread(b, 1, (size_t)n, f) != (size_t)n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    return b;
}

typedef struct bake_entry {
    int family, px, ydiv;
    sel_gui_atlas* at;
} bake_entry;

static int add_entry(bake_entry* e, int* n, int family, int px, int ydiv, sel_gui_atlas* at) {
    int j;
    if (at == NULL) {
        fprintf(stderr, "gui_fontbake: atlas %d/%d/%d failed\n", family, px, ydiv);
        return 0;
    }
    for (j = 0; j < *n; j++) {
        if (e[j].family == family && e[j].px == px && e[j].ydiv == ydiv) {
            fprintf(stderr, "gui_fontbake: duplicate atlas %d/%d/%d\n", family, px, ydiv);
            return 0;
        }
    }
    e[*n].family = family;
    e[*n].px = px;
    e[*n].ydiv = ydiv;
    e[(*n)++].at = at;
    return 1;
}

int main(int argc, char** argv) {
    static const int sizes[] = {DESK_FONT_SIZES};
    static const int scales[] = {DESK_FONT_BAKE_SCALES_Q16};
    static const int tall[] = {DESK_FONT_BAKE_TALL_PX};
    static const char* const amiga_names[] = {"mini4", "mini6", "mini8"};
    unsigned char* ttf[2];
    bake_entry ent[96];
    uint32_t packed_n[96];
    int pxs[64], npx = 0, ne = 0, w, i, k, a;
    FILE* o;
    if (argc < 4) {
        fprintf(stderr, "usage: gui_fontbake Inter-Regular.ttf Inter-SemiBold.ttf out.c [mini4|mini6|mini8:file ...]\n");
        return 2;
    }
    for (w = 0; w < 2; w++) {
        ttf[w] = slurp(argv[1 + w]);
        if (ttf[w] == NULL) {
            fprintf(stderr, "gui_fontbake: cannot read %s\n", argv[1 + w]);
            return 1;
        }
    }
    for (k = 0; k < (int)(sizeof scales / sizeof scales[0]); k++) {
        for (i = 0; i < DESK_FONT_SIZE_N; i++) {
            int px = (int)(((long long)sizes[i] * scales[k] + 32768) >> 16), j;
            for (j = 0; j < npx && pxs[j] != px; j++) {
            }
            if (j == npx) {
                pxs[npx++] = px;
            }
        }
    }
    for (w = 0; w < 2; w++) {
        for (i = 0; i < npx; i++) {
            if (!add_entry(ent, &ne, w, pxs[i], 1, desk_atlas_build_tall(ttf[w], pxs[i], 1))) {
                return 1;
            }
        }
    }
    /* I526: the tall-pixel (ydiv 2) Inter set is SemiBold headlines only, at scale 1.0. */
    for (i = 0; i < DESK_FONT_BAKE_TALL_PX_N; i++) {
        if (!add_entry(ent, &ne, DESK_FAMILY_INTER_SEMIBOLD, tall[i], 2, desk_atlas_build_tall(ttf[1], tall[i], 2))) {
            return 1;
        }
    }
    for (a = 4; a < argc; a++) {
        const char* colon = strchr(argv[a], ':');
        unsigned char* data;
        long len = 0;
        FILE* f;
        int fam = -1, ys;
        for (i = 0; colon != NULL && i < 3; i++) {
            if (strlen(amiga_names[i]) == (size_t)(colon - argv[a]) && strncmp(argv[a], amiga_names[i], (size_t)(colon - argv[a])) == 0) {
                fam = DESK_FAMILY_MINI4 + i;
            }
        }
        if (fam < 0) {
            fprintf(stderr, "gui_fontbake: bad amiga argument %s\n", argv[a]);
            return 2;
        }
        f = fopen(colon + 1, "rb");
        if (f == NULL) {
            fprintf(stderr, "gui_fontbake: cannot read %s\n", colon + 1);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = (unsigned char*)malloc((size_t)len);
        if (data == NULL || fread(data, 1, (size_t)len, f) != (size_t)len) {
            fprintf(stderr, "gui_fontbake: cannot read %s\n", colon + 1);
            return 1;
        }
        fclose(f);
        ys = desk_amiga_font_ysize(data, (size_t)len);
        if (!add_entry(ent, &ne, fam, ys, 2, desk_atlas_build_amiga(data, (size_t)len))) {
            return 1;
        }
        free(data);
    }
    o = fopen(argv[3], "w");
    if (o == NULL) {
        return 1;
    }
    fprintf(o, "/* Generated by tools/gui_fontbake.c from third_party/fonts (Inter OFL, Amiga mini fonts). Do not edit. */\n"
               "#include \"atlas_build.h\"\n\n");
    for (i = 0; i < ne; i++) {
        const sel_gui_atlas* at = ent[i].at;
        uint32_t g, n;
        fprintf(o, "static const sel_gui_glyph k_g%d[] = {\n", i);
        for (g = 0; g < at->glyph_n; g++) {
            const sel_gui_glyph* e = &at->glyphs[g];
            fprintf(o, "{%uu,%u,%u,%u,%u,%d,%d,%u},\n", (unsigned)e->cp, e->x, e->y, e->w, e->h, e->bx, e->by, e->adv);
        }
        fprintf(o, "};\nstatic const sel_gui_kern k_k%d[] = {\n", i);
        for (g = 0; g < at->kern_n; g++) {
            fprintf(o, "{%uu,%uu,%d},\n", (unsigned)at->kerns[g].a, (unsigned)at->kerns[g].b, at->kerns[g].amount);
        }
        if (at->kern_n == 0) {
            fprintf(o, "{0,0,0}\n");
        }
        n = (uint32_t)(at->page_w * at->page_h);
        /* I587: the page is stored packed by the icon packer; the round trip is checked here, per atlas. */
        {
            uint8_t* buf = (uint8_t*)malloc((size_t)n + (size_t)n / 128u + 2u);
            uint8_t* chk = (uint8_t*)malloc((size_t)n + 1u);
            size_t plen;
            if (buf == NULL || chk == NULL) {
                return 1;
            }
            plen = sel_gui_icon_pack(at->alpha, n, buf);
            if (!sel_gui_pack_decode(buf, plen, chk, n) || memcmp(chk, at->alpha, n) != 0) {
                fprintf(stderr, "gui_fontbake: atlas %d does not round trip through the packer\n", i);
                return 1;
            }
            fprintf(o, "};\nstatic const uint8_t k_a%d[] = {", i);
            for (g = 0; g < plen; g++) {
                fprintf(o, "%s%u,", g % 32 == 0 ? "\n" : "", (unsigned)buf[g]);
            }
            fprintf(o, "\n};\n");
            packed_n[i] = (uint32_t)plen;
            free(buf);
            free(chk);
        }
    }
    fprintf(o, "\nconst desk_baked_font sel_font_baked[] = {\n");
    for (i = 0; i < ne; i++) {
        const sel_gui_atlas* at = ent[i].at;
        fprintf(o, "{%d,%d,%d,{k_g%d,%uu,k_k%d,%uu,0,%d,%d,%d,%d,", ent[i].family, ent[i].px, ent[i].ydiv, i,
                (unsigned)at->glyph_n, i, (unsigned)at->kern_n, at->page_w, at->page_h, at->ascent, at->line_height);
        fprintf(o, "{%uu,%u,%u,%u,%u,%d,%d,%u}},k_a%d,%uu},\n", (unsigned)at->missing.cp, at->missing.x, at->missing.y,
                at->missing.w, at->missing.h, at->missing.bx, at->missing.by, at->missing.adv, i, (unsigned)packed_n[i]);
    }
    fprintf(o, "};\nconst uint32_t sel_font_baked_n = %d;\n", ne);
    fclose(o);
    return 0;
}
