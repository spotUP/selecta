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

/* I625a6a: a name that failed to open is probed once per list, not once per call. */
typedef struct {
    int calls[8];
    char seen[8][32];
    int n;
    int opens_name; /* index into seen order of the name that opens, matched by string; NULL none */
    const char* ok;
} miss;

static void* m_begin(void* ctx) { (void)ctx; return NULL; }
static void m_end(void* ctx, void* saved) { (void)ctx; (void)saved; }
static void* m_open(void* ctx, const char* name) {
    miss* m = ctx;
    int i;
    for (i = 0; i < m->n; i++) {
        if (strcmp(m->seen[i], name) == 0) {
            break;
        }
    }
    if (i == m->n) {
        strcpy(m->seen[m->n++], name);
    }
    m->calls[i]++;
    return (m->ok != NULL && strcmp(m->ok, name) == 0) ? (void*)0x55 : NULL;
}
static int m_count(const miss* m, const char* name) {
    int i;
    for (i = 0; i < m->n; i++) {
        if (strcmp(m->seen[i], name) == 0) {
            return m->calls[i];
        }
    }
    return 0;
}

static int miss_tests(void) {
    static const char* const l3[] = {"RAM:a", "Selecta:b", "FONTS:c", NULL};
    static const char* const l4[] = {"RAM:a", "Selecta:b", "FONTS:c", "FONTS:d", NULL};
    static const char* const l4r[] = {"FONTS:d", "RAM:a", "Selecta:b", "FONTS:c", NULL};
    miss m;
    mui_fontprobe_ops ops;
    int k, bad = 0;
    void* f = NULL;
#define MCHECK(c)                                                  \
    do {                                                           \
        if (!(c)) {                                                \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);     \
            bad++;                                                 \
        }                                                          \
    } while (0)
    memset(&m, 0, sizeof m);
    ops.ctx = &m; ops.quiet_begin = m_begin; ops.quiet_end = m_end; ops.open = m_open;
    mui_fontprobe_reset();
    for (k = 0; k < 20; k++) {
        f = mui_fontprobe_open(&ops, l3);
        MCHECK(f == NULL);
    }
    MCHECK(m_count(&m, "RAM:a") == 1);
    MCHECK(m_count(&m, "Selecta:b") == 1);
    MCHECK(m_count(&m, "FONTS:c") == 1);
    /* the list changes (a name added): every name of the new list is probed once more */
    for (k = 0; k < 20; k++) {
        mui_fontprobe_open(&ops, l4);
    }
    MCHECK(m_count(&m, "RAM:a") == 2 && m_count(&m, "Selecta:b") == 2 && m_count(&m, "FONTS:c") == 2 && m_count(&m, "FONTS:d") == 1);
    /* reorder counts as a change */
    for (k = 0; k < 5; k++) {
        mui_fontprobe_open(&ops, l4r);
    }
    MCHECK(m_count(&m, "RAM:a") == 3 && m_count(&m, "FONTS:d") == 2);
    /* explicit reset (live retheme) probes again */
    mui_fontprobe_reset();
    mui_fontprobe_open(&ops, l4r);
    MCHECK(m_count(&m, "FONTS:d") == 3);
    /* second name opens: first probed once, second opens every call, third never probed */
    memset(&m, 0, sizeof m);
    m.ok = "Selecta:b";
    mui_fontprobe_reset();
    for (k = 0; k < 10; k++) {
        f = mui_fontprobe_open(&ops, l3);
        MCHECK(f == (void*)0x55);
    }
    MCHECK(m_count(&m, "RAM:a") == 1);
    MCHECK(m_count(&m, "Selecta:b") == 10);
    MCHECK(m_count(&m, "FONTS:c") == 0);
    return bad;
}

int main(void) {
    rec r;
    mui_fontprobe_ops ops = {&r, q_begin, q_end, q_open};
    void* f;

    memset(&r, 0, sizeof r);
    mui_fontprobe_reset();
    r.succeed_at = 0; /* first name opens */
    f = mui_fontprobe_open(&ops, names);
    CHECK(f == (void*)0x77);
    CHECK(r.opens == 1 && r.begins == 1 && r.ends == 1 && r.open_outside == 0);
    CHECK(strcmp(r.order, "BoE") == 0);

    memset(&r, 0, sizeof r);
    mui_fontprobe_reset();
    r.succeed_at = 1; /* second name opens, third is never tried */
    f = mui_fontprobe_open(&ops, names);
    CHECK(f == (void*)0x77);
    CHECK(r.opens == 2 && r.begins == 1 && r.ends == 1 && r.open_outside == 0);
    CHECK(strcmp(r.order, "BooE") == 0);

    memset(&r, 0, sizeof r);
    mui_fontprobe_reset();
    r.succeed_at = -1; /* all fail */
    f = mui_fontprobe_open(&ops, names);
    CHECK(f == NULL);
    CHECK(r.opens == 3 && r.begins == 1 && r.ends == 1 && r.open_outside == 0);
    CHECK(strcmp(r.order, "BoooE") == 0);

    fails += miss_tests();
    if (fails == 0) {
        printf("mui-fontprobe: ok\n");
    }
    return fails != 0;
}
