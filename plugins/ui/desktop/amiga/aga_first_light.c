/* AGA first light probe (I439, M4-P6-1): opens the aga backend at its default size, draws an INDEX8 canvas with the
 * desktop UI palette (a gradient, theme bars and one moving box so later presents are dirty-rect bands), presents
 * 50 frames and prints the counters a rig check asserts:
 *   backend=aga depth=8 w=.. h=.. chip_bytes=.. c2p=kalms040|ref presents=.. c2p_calls=.. upload_pixels=.. ms=..
 * Ends early on Ctrl-C or the right mouse button. */
#include "backend_aga.h"

#include <selecta/gui/gui_draw.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int32_t w, h, x, y, f;
    uint32_t pal[256], t0, t1;
    sel_gui_canvas* c;
    uint8_t* px;
    desk_aga* b;
    desk_aga_default_size(&w, &h);
    b = desk_aga_open(w, h, 1);
    if (b == NULL) {
        printf("backend=aga open=failed\n");
        return 20;
    }
    w = desk_aga_width(b);
    h = desk_aga_height(b);
    c = (sel_gui_canvas*)calloc(1, sizeof *c);
    px = (uint8_t*)malloc((size_t)w * (size_t)h);
    if (c == NULL || px == NULL) {
        desk_aga_close(b);
        printf("backend=aga alloc=failed\n");
        return 20;
    }
    sel_gui_canvas_init(c, SEL_GUI_INDEX8, w, h, px);
    sel_gui_palette_default(pal, 0x1DB954u, 0x121212u, 0xFFFFFFu);
    sel_gui_set_palette(c, pal, 0, 256);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) px[(size_t)y * (size_t)w + (size_t)x] = (uint8_t)(3 + (x * 24) / w);
    }
    {
        sel_gui_rect all = {0, 0, w, h};
        c->dirty[0] = all;
        c->dirty_n = 1;
    }
    t0 = desk_aga_ticks();
    for (f = 0; f < 50; f++) {
        int32_t bx = (f * 8) % (w - 64), by = h / 2 - 16;
        if (f > 0) {
            sel_gui_rect r = {bx - 8 < 0 ? 0 : bx - 8, by, 80, 32};
            sel_gui_rect r2 = {bx - 8 < 0 ? 0 : bx - 8, 8, 80, 16};
            c->dirty[0] = r;
            c->dirty[1] = r2;
            c->dirty_n = 2;
        }
        for (y = 8; y < 24; y++) {
            for (x = (bx - 8 < 0 ? 0 : bx - 8); x < bx + 72 && x < w; x++) {
                px[(size_t)y * (size_t)w + (size_t)x] = (uint8_t)(x >= bx && x < bx + 64 ? 1 : 3 + (x * 24) / w);
            }
        }
        for (y = by; y < by + 32; y++) {
            for (x = (bx - 8 < 0 ? 0 : bx - 8); x < bx + 72 && x < w; x++) {
                px[(size_t)y * (size_t)w + (size_t)x] = (uint8_t)(x >= bx && x < bx + 64 ? 0 : 3 + (x * 24) / w);
            }
        }
        desk_aga_present(b, c);
        c->dirty_n = 0;
        if (desk_aga_poll(b, 20) == DESK_AGA_QUIT) break;
    }
    t1 = desk_aga_ticks();
    printf("backend=aga depth=%ld w=%ld h=%ld chip_bytes=%lu c2p=%s presents=%lu c2p_calls=%lu upload_pixels=%lu "
           "ms=%lu\n",
           (long)desk_aga_depth(b), (long)w, (long)h, (unsigned long)desk_aga_chip_held(b),
           desk_aga_c2p_kind(b) == DESK_AGA_C2P_KALMS_040 ? "kalms040" : "ref", (unsigned long)desk_aga_presents(b),
           (unsigned long)desk_aga_c2p_calls(b), (unsigned long)desk_aga_upload_pixels(b),
           (unsigned long)(t1 - t0));
    desk_aga_close(b);
    free(px);
    free(c);
    return 0;
}
