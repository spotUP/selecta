/* I369: sel_gui_image_scale must fit the 128 KB Amiga task stack. It used a ~165 KB sel_gui_canvas local, which
 * overflowed the swapped stack when the art job ran inline on the UI task and crashed Selecta at exit. The
 * scaler runs here on a 64 KB thread stack: a canvas local overflows into the guard page and the test dies. */
#include <selecta/gui/gui_image.h>

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define STACK_BYTES (64u * 1024u)

static uint32_t g_src[64 * 64];
static uint32_t g_dst[8 * 8];

static void* run(void* arg) {
    int i;
    (void)arg;
    for (i = 0; i < 64 * 64; i++) {
        g_src[i] = 0xFF204060u;
    }
    sel_gui_image_scale(g_src, 64, 64, g_dst, 8, 8);
    return NULL;
}

int main(void) {
    pthread_attr_t attr;
    pthread_t t;

    pthread_attr_init(&attr);
    if (pthread_attr_setstacksize(&attr, STACK_BYTES) != 0 || pthread_create(&t, &attr, run, NULL) != 0) {
        puts("FAIL: cannot start the small-stack thread");
        return 1;
    }
    pthread_join(t, NULL);
    if (g_dst[0] != 0xFF204060u || g_dst[63] != 0xFF204060u) {
        printf("FAIL: scaled pixel %08x\n", (unsigned)g_dst[0]);
        return 1;
    }
    puts("OK image_scale_stack");
    return 0;
}
