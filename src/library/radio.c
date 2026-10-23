/* RADIO steering (design doc: queue "radio mode appends from a seed query"; plan 2026-10-25-m3-library.md,
 * Phase 8, I186, D-M3-10). The next song is drawn from two existing queries, never from a new SQL path:
 *   - sel_library_similar(seed): rows with a genre match, or a rating of 4 or more, are candidates;
 *   - sel_library_also_by(seed): the seed composer's other songs are candidates.
 * Weights steer the draw: a genre match and a composer match add 1 each, a rating of 4 or 5 adds 1 or 2 on
 * top of the base 1. Skip-flagged songs (sel_library_flag_skipped, I171) are never candidates. The caller
 * supplies the draw (the engine steps sel_xorshift32), so one seed gives one sequence. Two passes over the same queries
 * (total weight, then the pick) keep the function free of any candidate array. */
#include <stdint.h>
#include <string.h>

#include "db.h"
#include "source.h"

#define SEL_RADIO_MIN_RATING 4

typedef struct radio_pass {
    sel_library* lib;
    const char* avoid; /* queue path of the previous pick, left out unless nothing else is left */
    uint32_t total;    /* pass 1: sum of the weights */
    uint32_t target;   /* pass 2: weight offset of the pick */
    char* out;         /* pass 2: queue path of the pick */
    size_t cap;
    int picked;
} radio_pass;

/* The form the queue holds for a library key: the plain path of a local song, the key of any other. */
static void queue_form(const char* key, char* out, size_t cap) {
    char src[32], path[512];

    if (sel_song_key_split(key, src, sizeof src, path, sizeof path) && strcmp(src, "local") == 0) {
        strncpy(out, path, cap - 1u);
    } else {
        strncpy(out, key, cap - 1u);
    }
    out[cap - 1u] = '\0';
}

/* Weight of one candidate, 0 when it is excluded (skip flag, or the avoided previous pick). */
static uint32_t admit(radio_pass* p, const char* key, uint32_t weight, char* form, size_t cap) {
    queue_form(key, form, cap);
    if (weight == 0 || (p->avoid != NULL && strcmp(p->avoid, form) == 0)) return 0;
    if (sel_library_flag_skipped(p->lib, key)) return 0;
    return weight;
}

static void take(radio_pass* p, const char* key, uint32_t weight) {
    char form[512];

    weight = admit(p, key, weight, form, sizeof form);
    if (weight == 0) return;
    if (p->out == NULL) {
        p->total += weight;
    } else if (!p->picked) {
        if (p->target < weight) {
            strncpy(p->out, form, p->cap - 1u);
            p->out[p->cap - 1u] = '\0';
            p->picked = 1;
        } else {
            p->target -= weight;
        }
    }
}

static void on_similar(void* ctx, const char* key, const char* name, const char* genre, int rating, int match) {
    uint32_t w = 0;

    (void)name;
    (void)genre;
    if (match || rating >= SEL_RADIO_MIN_RATING) {
        w = 1u + (match ? 1u : 0u) + (rating >= SEL_RADIO_MIN_RATING ? (uint32_t)(rating - SEL_RADIO_MIN_RATING + 1) : 0u);
    }
    take((radio_pass*)ctx, key, w);
}

static void on_composer(void* ctx, const char* key, const char* name, const char* display) {
    (void)name;
    (void)display;
    take((radio_pass*)ctx, key, 2u);
}

static int run_pass(radio_pass* p, const char* seed) {
    int a = sel_library_similar(p->lib, seed, on_similar, p);
    int b = sel_library_also_by(p->lib, seed, on_composer, p);

    return a > 0 || b > 0;
}

int sel_library_radio_next(sel_library* lib, const char* seed, uint32_t draw, const char* avoid, char* out, size_t cap) {
    radio_pass p;
    char use[SEL_SONG_KEY_MAX];
    int attempt;

    if (lib == NULL || seed == NULL || out == NULL || cap < 2u || !sel_song_key_of(seed, use, sizeof use)) return 0;
    for (attempt = 0; attempt < 2; attempt++) {
        memset(&p, 0, sizeof p);
        p.lib = lib;
        p.avoid = attempt == 0 ? avoid : NULL;
        run_pass(&p, use);
        if (p.total > 0) break;
    }
    if (p.total == 0) return 0;
    p.target = draw % p.total;
    p.out = out;
    p.cap = cap;
    run_pass(&p, use);
    return p.picked;
}
