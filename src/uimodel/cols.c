/* The column splitter: see include/selecta/uimodel/cols.h (I1079a). */
#include <selecta/uimodel/cols.h>

static uint32_t g_calls;

uint32_t sel_uim_cols_calls(void) { return g_calls; }

uint32_t sel_uim_cols_split(int32_t row_w, int32_t gap, const sel_uim_col* c, uint32_t n, int32_t* x, int32_t* w) {
    int pinned[SEL_UIM_COLS_MAX] = {0};
    int32_t room, rem, wsum, minsum = 0, pos = 0, used = 0, lastflex = -1;
    uint32_t i, pass, present = 0;
    int changed;
    g_calls++;
    if (n > SEL_UIM_COLS_MAX) {
        n = SEL_UIM_COLS_MAX;
    }
    room = row_w;
    for (i = 0; i < n; i++) {
        w[i] = 0;
        if (c[i].weight > 0 || c[i].fixed > 0) {
            present++;
            if (c[i].weight > 0) {
                lastflex = (int32_t)i;
                minsum += c[i].min_w > 0 ? c[i].min_w : 0;
            } else {
                room -= c[i].fixed;
                w[i] = c[i].fixed;
            }
        }
    }
    if (present > 1) {
        room -= (int32_t)(present - 1) * gap;
    }
    if (room < 0) {
        room = 0;
    }
    if (minsum > room) {
        /* Not enough room for the minimums: shrink them in proportion. */
        for (i = 0; i < n; i++) {
            if (c[i].weight > 0) {
                w[i] = c[i].min_w > 0 ? (int32_t)((int64_t)c[i].min_w * room / minsum) : 0;
            }
        }
    } else {
        for (pass = 0; pass <= n; pass++) {
            rem = room;
            wsum = 0;
            for (i = 0; i < n; i++) {
                if (c[i].weight > 0) {
                    if (pinned[i]) {
                        rem -= w[i];
                    } else {
                        wsum += c[i].weight;
                    }
                }
            }
            changed = 0;
            for (i = 0; i < n && wsum > 0; i++) {
                if (c[i].weight > 0 && !pinned[i]) {
                    w[i] = (int32_t)((int64_t)rem * c[i].weight / wsum);
                }
            }
            for (i = 0; i < n; i++) {
                if (c[i].weight > 0 && !pinned[i] && w[i] < c[i].min_w) {
                    w[i] = c[i].min_w;
                    pinned[i] = 1;
                    changed = 1;
                }
            }
            if (!changed) {
                for (i = 0; i < n; i++) {
                    if (c[i].weight > 0 && !pinned[i] && c[i].max_w > 0 && w[i] > c[i].max_w) {
                        w[i] = c[i].max_w;
                        pinned[i] = 1;
                        changed = 1;
                    }
                }
            }
            if (!changed) {
                break;
            }
        }
    }
    /* The integer remainder lands on the last flex column that can take it. */
    for (i = 0; i < n; i++) {
        if (c[i].weight > 0) {
            used += w[i];
        }
    }
    if (lastflex >= 0 && used < room) {
        int32_t k, target = lastflex;
        for (k = lastflex; k >= 0; k--) {
            if (c[k].weight > 0 && (c[k].max_w <= 0 || w[k] + (room - used) <= c[k].max_w)) {
                target = k;
                break;
            }
        }
        w[target] += room - used;
    }
    for (i = 0; i < n; i++) {
        x[i] = pos;
        if (c[i].weight > 0 || c[i].fixed > 0) {
            pos += w[i] + gap;
        }
    }
    return present;
}
