/* I439 (M4-P6-1): Kalms' c2p1x1_8_c5_040 agrees with the C reference desk_aga_c2p_ref bit for bit, for the whole
 * screen and for row bands (scroffsy), on a 68k CPU (vamos -C 68040). The planes outside a band stay untouched.
 * Prints "c2p_kalms040=match bands=N" and returns 0, else the first mismatch and 20. */
#include "../../plugins/ui/desktop/amiga/aga_planar.h"
#include "../../third_party/kalms-c2p/normal/c2p1x1_8_c5_040.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 320
#define H 64

static uint32_t seed = 12345u;
static uint8_t next(void) {
    seed = seed * 1103515245u + 12345u;
    return (uint8_t)(seed >> 16);
}

int main(void) {
    static uint8_t chunky[W * H], ref[W * H], got[W * H];
    static const int32_t bands[][2] = {{0, H}, {0, 1}, {5, 7}, {17, 32}, {63, 1}};
    uint32_t bplsize = (W / 8) * H, i, k;
    for (i = 0; i < W * H; i++) chunky[i] = next();
    for (k = 0; k < sizeof bands / sizeof bands[0]; k++) {
        int32_t y = bands[k][0], rows = bands[k][1];
        memset(ref, 0xA5, sizeof ref);
        memset(got, 0xA5, sizeof got);
        desk_aga_c2p_ref(chunky, W, y, rows, ref, bplsize);
        c2p1x1_8_c5_040_init(W, rows, y, (int)bplsize);
        c2p1x1_8_c5_040(chunky + (size_t)y * W, got);
        for (i = 0; i < sizeof ref; i++) {
            if (ref[i] != got[i]) {
                printf("c2p_kalms040=mismatch band=%ld,%ld byte=%lu ref=%02x got=%02x\n", (long)y, (long)rows,
                       (unsigned long)i, ref[i], got[i]);
                return 20;
            }
        }
    }
    printf("c2p_kalms040=match bands=%lu\n", (unsigned long)k);
    return 0;
}
