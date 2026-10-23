/* I196d: the recording split walker (src/library/record_name.c sel_record_cut_*). A split asked for mid-frame must
 * land on the next MP3 frame or Ogg page start, every byte must pass through once, in order. */
#include "record_name.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            failures++;                                                   \
        }                                                                 \
    } while (0)

#define CAP 80000u

typedef struct {
    uint8_t out[CAP];
    size_t n;
    size_t split_at[4];
    unsigned splits;
} sink;

static sink k;
static uint8_t in[CAP];
static sel_record_cut cut;

static void collect(void* ctx, const uint8_t* p, size_t n, int split) {
    sink* s = ctx;
    if (split && s->splits < 4u) {
        s->split_at[s->splits++] = s->n;
    }
    memcpy(s->out + s->n, p, n);
    s->n += n;
}

/* Feeds in[from, to) in chunks of `step` bytes. */
static void feed(size_t from, size_t to, size_t step) {
    while (from < to) {
        size_t n = to - from < step ? to - from : step;
        sel_record_cut_feed(&cut, in + from, n, collect, &k);
        from += n;
    }
}

static void start(const char* ext) {
    memset(&k, 0, sizeof k);
    sel_record_cut_init(&cut, ext);
}

/* Every input byte came out once, in order. */
static int whole(size_t len) {
    sel_record_cut_flush(&cut, collect, &k);
    return k.n == len && memcmp(k.out, in, len) == 0;
}

/* MPEG 1 Layer III, 64 kbit/s, 44100 Hz, no padding: 144 * 64000 / 44100 = 208 bytes per frame. */
static size_t mpeg_frames(size_t at, unsigned count) {
    unsigned i;
    for (i = 0; i < count; i++, at += 208u) {
        memset(in + at, 0x00, 208u);
        in[at] = 0xFF;
        in[at + 1u] = 0xFB;
        in[at + 2u] = 0x50;
        in[at + 3u] = 0x64;
    }
    return at;
}

/* Ogg pages of two segments (100 + 50 bytes): 27 + 2 + 150 = 179 bytes per page. */
static size_t ogg_pages(size_t at, unsigned count) {
    unsigned i;
    for (i = 0; i < count; i++, at += 179u) {
        memset(in + at, 0x00, 179u);
        memcpy(in + at, "OggS", 4);
        in[at + 26u] = 2u;
        in[at + 27u] = 100u;
        in[at + 28u] = 50u;
    }
    return at;
}

int main(void) {
    size_t len;

    /* MPEG: asked for inside frame 1's body, the split lands on frame 2 (offset 416). */
    len = mpeg_frames(0, 10);
    start("mp3");
    feed(0, 300, 7);
    sel_record_cut_request(&cut);
    feed(300, len, 7);
    CHECK(k.splits == 1u && k.split_at[0] == 416u);
    CHECK(whole(len));

    /* MPEG: asked for while frame 1's header is half held (2 of 4 bytes), the split waits for frame 2. */
    start("mp3");
    feed(0, 210, 210);
    sel_record_cut_request(&cut);
    feed(210, len, 1000);
    CHECK(k.splits == 1u && k.split_at[0] == 416u);
    CHECK(whole(len));

    /* MPEG: a split exactly at a frame start, asked for before its header arrived. */
    start("mp2");
    feed(0, 208, 64);
    sel_record_cut_request(&cut);
    feed(208, len, 64);
    CHECK(k.splits == 1u && k.split_at[0] == 208u);
    CHECK(whole(len));

    /* MPEG: garbage before the first frame is passed on; the split lands on the first frame after it. */
    memset(in, 0x11, 5u);
    len = mpeg_frames(5, 4);
    start("mp3");
    sel_record_cut_request(&cut);
    feed(0, len, 3);
    CHECK(k.splits == 1u && k.split_at[0] == 5u);
    CHECK(whole(len));

    /* MPEG: no frame start for SEL_RECORD_CUT_GIVE_UP bytes: the split falls on the next byte instead. */
    memset(in, 0x00, 70000u);
    start("mp3");
    sel_record_cut_request(&cut);
    feed(0, 70000u, 4096);
    CHECK(k.splits == 1u && k.split_at[0] == SEL_RECORD_CUT_GIVE_UP);
    CHECK(whole(70000u));

    /* Ogg: asked for inside page 2's body, the split lands on page 3 (offset 358). */
    len = ogg_pages(0, 6);
    start("ogg");
    feed(0, 250, 11);
    sel_record_cut_request(&cut);
    feed(250, len, 11);
    CHECK(k.splits == 1u && k.split_at[0] == 358u);
    CHECK(whole(len));

    /* Ogg: asked for while page 2's header is held (20 of 29 bytes), the split waits for page 3. */
    start("opus");
    feed(0, 199, 199);
    sel_record_cut_request(&cut);
    feed(199, len, 500);
    CHECK(k.splits == 1u && k.split_at[0] == 358u);
    CHECK(whole(len));

    /* Any other format has no frame walk: the split falls on the next byte. */
    len = 100u;
    memset(in, 0x22, len);
    start("aac");
    feed(0, 40, 40);
    sel_record_cut_request(&cut);
    feed(40, len, 40);
    CHECK(k.splits == 1u && k.split_at[0] == 40u);
    CHECK(whole(len));

    if (failures != 0) {
        printf("record_cut_unit FAILED %d\n", failures);
        return 1;
    }
    printf("record_cut_unit_ok\n");
    return 0;
}
