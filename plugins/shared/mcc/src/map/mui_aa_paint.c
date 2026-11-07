/* mui_aa_paint: see mui_aa_paint.h. */
#include "mui_aa_paint.h"

#include "mui_aa.h"

void mui_aa_paint_rect(const mui_aa_paint_ops* ops, int x0, int y0, int x1, int y1, uint32_t rgb) {
    long pen;
    if (x1 < x0 || y1 < y0) {
        return;
    }
    if (ops->tc) {
        ops->rect_rgb(ops->ctx, x0, y0, x1, y1, rgb);
        return;
    }
    pen = ops->pen(ops->ctx, rgb);
    if (pen >= 0) {
        ops->rect_pen(ops->ctx, x0, y0, x1, y1, pen);
    }
}

void mui_aa_paint_round_fill(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb) {
    int row;
    if (radius <= 0) {
        mui_aa_paint_rect(ops, x, y, x + w - 1, y + h - 1, rgb);
        return;
    }
    for (row = 0; row < h; row++) {
        int in = mui_aa_round_inset(w, h, radius, row);
        mui_aa_paint_rect(ops, x + in, y + row, x + w - 1 - in, y + row, rgb);
    }
}

void mui_aa_paint_round_fill_hole(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb, int hx, int hy, int hw, int hh) {
    int row;
    for (row = 0; row < h; row++) {
        int in = radius > 0 ? mui_aa_round_inset(w, h, radius, row) : 0, l = x + in, r = x + w - 1 - in, py = y + row;
        if (py < hy || py >= hy + hh) {
            mui_aa_paint_rect(ops, l, py, r, py, rgb);
            continue;
        }
        if (l < hx) {
            mui_aa_paint_rect(ops, l, py, hx - 1 < r ? hx - 1 : r, py, rgb);
        }
        if (r >= hx + hw) {
            mui_aa_paint_rect(ops, l > hx + hw ? l : hx + hw, py, r, py, rgb);
        }
    }
}

void mui_aa_paint_round_corners(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb) {
    int row;
    if (radius <= 0) {
        return;
    }
    for (row = 0; row < h; row++) {
        int in = mui_aa_round_inset(w, h, radius, row);
        if (in > 0) {
            mui_aa_paint_rect(ops, x, y + row, x + in - 1, y + row, rgb);
            mui_aa_paint_rect(ops, x + w - in, y + row, x + w - 1, y + row, rgb);
        }
    }
}

void mui_aa_paint_round_frame(const mui_aa_paint_ops* ops, int x, int y, int w, int h, int radius, uint32_t rgb) {
    int row;
    for (row = 0; row < h; row++) {
        int in = mui_aa_round_inset(w, h, radius, row);
        if (radius <= 0 || row == 0 || row == h - 1) {
            mui_aa_paint_rect(ops, x + in, y + row, x + w - 1 - in, y + row, rgb);
        } else {
            /* Join to the neighbour row nearer the edge so the corner stays closed. */
            int up = mui_aa_round_inset(w, h, radius, row < h / 2 ? row - 1 : row + 1);
            int end = up - 1 > in ? up - 1 : in;
            mui_aa_paint_rect(ops, x + in, y + row, x + end, y + row, rgb);
            mui_aa_paint_rect(ops, x + w - 1 - end, y + row, x + w - 1 - in, y + row, rgb);
        }
    }
}

void mui_aa_paint_runs(const mui_aa_paint_ops* ops, int x, int y, const unsigned char* slot, int n, const uint32_t* slot_rgb) {
    int px;
    for (px = 0; px < n;) {
        int idx = slot[px], run = px + 1;
        while (run < n && slot[run] == idx) {
            run++;
        }
        if (idx > 0) {
            mui_aa_paint_rect(ops, x + px, y, x + run - 1, y, slot_rgb[idx]);
        }
        px = run;
    }
}
