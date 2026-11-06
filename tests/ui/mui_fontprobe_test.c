/* I625a3: the font probe runs every open between one quiet begin and one quiet end (invariant I214: a probe of a name on a
 * missing assign never raises a requester). Pure host test of map/mui_fontprobe.c, which mui_aa_draw.c's font_get calls. */
#include <stdio.h>
#include <string.h>

#include "map/mui_fontprobe.h"

typedef struct {
    int begins, ends, opens, depth;
    int open_outside; /* opens seen while not between begin and end */
    int succeed_at;   /* index of the name that opens, -1 for none */
    char order[64];
} rec;

static void* q_begin(void* ctx) {
    rec* r = ctx;
    r->begins++;
    r->depth++;
    strcat(r->order, "B");
    return (void*)0x1234;
}
static void q_end(void* ctx, void* saved) {
    rec* r = ctx;
    r->ends++;
    r->depth--;
    strcat(r->order, "E");
    if (saved != (void*)0x1234) {
        r->open_outside += 100; /* the token from begin must come back */
    }
}
static void* q_open(void* ctx, const char* name) {
    rec* r = ctx;
    (void)name;
    if (r->depth != 1) {
        r->open_outside++;
    }
    strcat(r->order, "o");
    return r->opens++ == r->succeed_at ? (void*)0x77 : NULL;
}

static int fails;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            printf("FAIL %s:%d %s order=%s\n", __FILE__, __LINE__, #c, r.order); \
            fails++;                                                     \
        }                                                                \
    } while (0)

static const char* const names[] = {"RAM:a", "Selecta:b", "FONTS:c", NULL};

int main(void) {
    rec r;
    mui_fontprobe_ops ops = {&r, q_begin, q_end, q_open};
    void* f;

    memset(&r, 0, sizeof r);
    r.succeed_at = 0; /* first name opens */
    f = mui_fontprobe_open(&ops, names);
    CHECK(f == (void*)0x77);
    CHECK(r.opens == 1 && r.begins == 1 && r.ends == 1 && r.open_outside == 0);
    CHECK(strcmp(r.order, "BoE") == 0);

    memset(&r, 0, sizeof r);
    r.succeed_at = 1; /* second name opens, third is never tried */
    f = mui_fontprobe_open(&ops, names);
    CHECK(f == (void*)0x77);
    CHECK(r.opens == 2 && r.begins == 1 && r.ends == 1 && r.open_outside == 0);
    CHECK(strcmp(r.order, "BooE") == 0);

    memset(&r, 0, sizeof r);
    r.succeed_at = -1; /* all fail */
    f = mui_fontprobe_open(&ops, names);
    CHECK(f == NULL);
    CHECK(r.opens == 3 && r.begins == 1 && r.ends == 1 && r.open_outside == 0);
    CHECK(strcmp(r.order, "BoooE") == 0);

    if (fails == 0) {
        printf("mui-fontprobe: ok\n");
    }
    return fails != 0;
}
