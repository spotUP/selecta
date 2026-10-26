/* Integer median-cut quantiser for the INDEX8 artwork sub-palettes (design doc 2.7, DUI-P6-4). No floats. */
#include <selecta/gui/gui_image.h>

#define QMAX_SLOTS 64u

typedef struct {
    uint32_t start, len;
} qbox;

static uint32_t chan(uint32_t px, uint32_t ch) { return (px >> (16u - 8u * ch)) & 255u; }

/* Insertion sort of w[lo .. lo+len-1] by one channel (the boxes are small: at most SEL_GUI_QUANT_MAX words). */
static void sort_by(uint32_t* w, uint32_t lo, uint32_t len, uint32_t ch) {
    uint32_t i, j, v;
    for (i = lo + 1u; i < lo + len; i++) {
        v = w[i];
        for (j = i; j > lo && chan(w[j - 1u], ch) > chan(v, ch); j--) {
            w[j] = w[j - 1u];
        }
        w[j] = v;
    }
}

/* The channel with the widest range in a box, and that range. */
static uint32_t widest(const uint32_t* w, const qbox* b, uint32_t* range) {
    uint32_t ch, i, best = 0u;
    *range = 0u;
    for (ch = 0; ch < 3u; ch++) {
        uint32_t lo = 255u, hi = 0u;
        for (i = b->start; i < b->start + b->len; i++) {
            uint32_t v = chan(w[i], ch);
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        if (hi - lo > *range) {
            *range = hi - lo;
            best = ch;
        }
    }
    return best;
}

uint32_t sel_gui_quantise(const uint32_t* px, uint32_t n, uint32_t* out, uint32_t slots) {
    uint32_t w[SEL_GUI_QUANT_MAX];
    qbox box[QMAX_SLOTS];
    uint32_t boxes = 1u, i, k;
    if (n == 0u || slots == 0u) {
        return 0u;
    }
    if (n > SEL_GUI_QUANT_MAX) {
        n = SEL_GUI_QUANT_MAX;
    }
    if (slots > QMAX_SLOTS) {
        slots = QMAX_SLOTS;
    }
    for (i = 0; i < n; i++) {
        w[i] = px[i] & 0xFFFFFFu;
    }
    box[0].start = 0u;
    box[0].len = n;
    while (boxes < slots) {
        uint32_t pick = boxes, most = 0u, range, ch, half;
        for (k = 0; k < boxes; k++) { /* the largest box that still has two different colours */
            uint32_t r;
            if (box[k].len > most && box[k].len > 1u) {
                widest(w, &box[k], &r);
                if (r > 0u) {
                    pick = k;
                    most = box[k].len;
                }
            }
        }
        if (pick == boxes) {
            break;
        }
        ch = widest(w, &box[pick], &range);
        sort_by(w, box[pick].start, box[pick].len, ch);
        half = box[pick].len / 2u;
        box[boxes].start = box[pick].start + half;
        box[boxes].len = box[pick].len - half;
        box[pick].len = half;
        boxes++;
    }
    for (k = 0; k < boxes; k++) {
        uint32_t sum[3] = {0u, 0u, 0u};
        for (i = box[k].start; i < box[k].start + box[k].len; i++) {
            sum[0] += chan(w[i], 0);
            sum[1] += chan(w[i], 1);
            sum[2] += chan(w[i], 2);
        }
        out[k] = (((sum[0] + box[k].len / 2u) / box[k].len) << 16) | (((sum[1] + box[k].len / 2u) / box[k].len) << 8) |
                 ((sum[2] + box[k].len / 2u) / box[k].len);
    }
    return boxes;
}
