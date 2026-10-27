/* Amiga bitmap font reader (I526-P1); see atlas_build.h. Reads the loadable disk font (HUNK_HEADER, one HUNK_CODE
 * holding a DiskFontHeader and a TextFont) and returns the same atlas the stb builder returns, so a bitmap font
 * goes through the existing bake and text path unchanged (alpha 0 or 255). Runs on the host at build time only. */
#include "atlas_build.h"

#include <stdlib.h>
#include <string.h>

#define PAGE_W 1024

static uint32_t be32(const unsigned char* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint32_t be16(const unsigned char* p) { return ((uint32_t)p[0] << 8) | p[1]; }

/* Offsets inside the code hunk body (the moveq/rts stub is body[0..3]; pointers are relative to body[0]). */
#define DFH_NODE 4
#define DFH_FILEID (DFH_NODE + 14)
#define TF_AT (DFH_NODE + 14 + 2 + 2 + 4 + 32) /* the TextFont follows the DiskFontHeader */
#define TF_YSIZE (TF_AT + 20)
#define TF_XSIZE (TF_AT + 24)
#define TF_BASELINE (TF_AT + 26)
#define TF_LOCHAR (TF_AT + 32)
#define TF_HICHAR (TF_AT + 33)
#define TF_CHARDATA (TF_AT + 34)
#define TF_MODULO (TF_AT + 38)
#define TF_CHARLOC (TF_AT + 40)
#define TF_CHARSPACE (TF_AT + 44)
#define TF_CHARKERN (TF_AT + 48)
#define TF_END (TF_AT + 52)

int desk_amiga_font_ysize(const unsigned char* data, size_t n) {
    if (n < 0x20u + TF_END || be32(data) != 0x3F3u || be32(data + 0x18) != 0x3E9u) {
        return 0;
    }
    return (int)be16(data + 0x20 + TF_YSIZE);
}

sel_gui_atlas* desk_atlas_build_amiga(const unsigned char* data, size_t n) {
    const unsigned char *b, *cdata, *cloc, *cspace, *ckern;
    size_t body_n;
    int ysize, xsize, baseline, lo, hi, nch, row = 0, px = 0, py = 0, pass, c;
    uint32_t modulo, ofs;
    sel_gui_atlas* at;
    sel_gui_glyph* glyphs;
    unsigned char* page = NULL;
    if (desk_amiga_font_ysize(data, n) <= 0) {
        return NULL;
    }
    b = data + 0x20;
    body_n = n - 0x20u;
    if (be16(b + DFH_FILEID) != 0x0F80u) {
        return NULL;
    }
    ysize = (int)be16(b + TF_YSIZE);
    xsize = (int)be16(b + TF_XSIZE);
    baseline = (int)be16(b + TF_BASELINE);
    lo = b[TF_LOCHAR];
    hi = b[TF_HICHAR];
    modulo = be16(b + TF_MODULO);
    ofs = be32(b + TF_CHARDATA);
    if (hi < lo || ofs + (uint32_t)ysize * modulo > body_n || be32(b + TF_CHARLOC) + (uint32_t)(hi - lo + 2) * 4u > body_n) {
        return NULL;
    }
    cdata = b + ofs;
    cloc = b + be32(b + TF_CHARLOC);
    cspace = be32(b + TF_CHARSPACE) != 0u ? b + be32(b + TF_CHARSPACE) : NULL;
    ckern = be32(b + TF_CHARKERN) != 0u ? b + be32(b + TF_CHARKERN) : NULL;
    nch = hi - lo + 1;
    at = (sel_gui_atlas*)calloc(1, sizeof *at);
    glyphs = (sel_gui_glyph*)calloc((size_t)nch, sizeof *glyphs);
    if (at == NULL || glyphs == NULL) {
        free(at);
        free(glyphs);
        return NULL;
    }
    /* Layout units are ydiv 2 (a tall pixel is two layout units tall): rows count twice. */
    at->ascent = (baseline + 1) * 2;
    at->line_height = ysize * 2;
    for (pass = 0; pass < 2; pass++) {
        px = py = row = 0;
        for (c = 0; c <= nch; c++) { /* nch entries, then the missing glyph (the extra CharLoc entry) */
            uint32_t loc = be32(cloc + 4 * c), bit0 = loc >> 16, w = loc & 0xFFFFu, r, x;
            sel_gui_glyph* g = c == nch ? &at->missing : &glyphs[c];
            memset(g, 0, sizeof *g);
            g->cp = c == nch ? 0xFFFFFFFFu : (uint32_t)(lo + c);
            g->adv = (uint16_t)(cspace != NULL && c < nch ? (int)(int16_t)be16(cspace + 2 * c) : xsize);
            g->bx = (int16_t)(ckern != NULL && c < nch ? (int)(int16_t)be16(ckern + 2 * c) : 0);
            g->by = (int16_t)(-(baseline + 1) * 2);
            if (w == 0u || (bit0 + w + 7u) / 8u > modulo) {
                continue;
            }
            if (px + (int)w + 1 > PAGE_W) {
                px = 0;
                py += row + 1;
                row = 0;
            }
            g->x = (uint16_t)px;
            g->y = (uint16_t)py;
            g->w = (uint16_t)w;
            g->h = (uint16_t)ysize;
            if (page != NULL) {
                for (r = 0; r < (uint32_t)ysize; r++) {
                    for (x = 0; x < w; x++) {
                        uint32_t bit = bit0 + x;
                        if ((cdata[r * modulo + bit / 8u] >> (7u - (bit & 7u))) & 1u) {
                            page[(size_t)(py + (int)r) * PAGE_W + (size_t)(px + (int)x)] = 255u;
                        }
                    }
                }
            }
            px += (int)w + 1;
            if (ysize > row) {
                row = ysize;
            }
        }
        if (pass == 0) {
            at->page_w = PAGE_W;
            at->page_h = py + row + 1;
            page = (unsigned char*)calloc((size_t)at->page_w * (size_t)at->page_h, 1);
            if (page == NULL) {
                free(at);
                free(glyphs);
                return NULL;
            }
        }
    }
    at->glyphs = glyphs;
    at->glyph_n = (uint32_t)nch;
    at->kerns = (sel_gui_kern*)calloc(1, sizeof(sel_gui_kern));
    at->kern_n = 0;
    at->alpha = page;
    return at;
}
