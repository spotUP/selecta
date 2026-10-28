/* Shared multi-selection model: see include/selecta/uimodel/select.h. Runs are sorted, disjoint and never adjacent. */
#include <selecta/ranges.h>
#include <selecta/uimodel/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Makes room for `extra` more runs; 0 (nothing changed) when the heap refuses. */
static int runs_reserve(sel_uim_selset* s, sel_uim_runs* r, uint32_t extra) {
    uint32_t cap = r->cap;
    sel_uim_run* grown;
    if (r->n + extra <= cap) return 1;
    if (cap == 0u) cap = 8u;
    while (cap < r->n + extra) cap *= 2u;
    grown = (sel_uim_run*)realloc(r->v, (size_t)cap * sizeof *grown);
    if (grown == NULL) return 0;
    r->v = grown;
    r->cap = cap;
    s->allocs++;
    return 1;
}

static void runs_release(sel_uim_runs* r) {
    free(r->v);
    r->v = NULL;
    r->n = r->cap = 0;
}

/* Adds lo..hi (reserved by the caller: n + 1 runs), merging touching and overlapping runs. */
static void runs_add(sel_uim_runs* r, uint32_t lo, uint32_t hi) {
    uint32_t i = 0, j;
    while (i < r->n && (uint64_t)r->v[i].hi + 1u < lo) i++;
    j = i;
    while (j < r->n && r->v[j].lo <= (uint64_t)hi + 1u) {
        if (r->v[j].lo < lo) lo = r->v[j].lo;
        if (r->v[j].hi > hi) hi = r->v[j].hi;
        j++;
    }
    if (j == i) {
        memmove(&r->v[i + 1], &r->v[i], (size_t)(r->n - i) * sizeof r->v[0]);
        r->n++;
    } else if (j > i + 1u) {
        memmove(&r->v[i + 1], &r->v[j], (size_t)(r->n - j) * sizeof r->v[0]);
        r->n -= j - i - 1u;
    }
    r->v[i].lo = lo;
    r->v[i].hi = hi;
}

/* Removes index x (reserved by the caller: n + 1 runs, a split adds one). */
static void runs_remove(sel_uim_runs* r, uint32_t x) {
    uint32_t i;
    for (i = 0; i < r->n; i++) {
        if (x < r->v[i].lo || x > r->v[i].hi) continue;
        if (r->v[i].lo == r->v[i].hi) {
            memmove(&r->v[i], &r->v[i + 1], (size_t)(r->n - i - 1u) * sizeof r->v[0]);
            r->n--;
        } else if (x == r->v[i].lo) {
            r->v[i].lo++;
        } else if (x == r->v[i].hi) {
            r->v[i].hi--;
        } else {
            memmove(&r->v[i + 1], &r->v[i], (size_t)(r->n - i) * sizeof r->v[0]);
            r->v[i].hi = x - 1u;
            r->v[i + 1u].lo = x + 1u;
            r->n++;
        }
        return;
    }
}

static int runs_has(const sel_uim_runs* r, uint32_t x) {
    uint32_t lo = 0, hi = r->n;
    while (lo < hi) { /* binary search over the sorted runs */
        uint32_t mid = lo + (hi - lo) / 2u;
        if (x < r->v[mid].lo) hi = mid;
        else if (x > r->v[mid].hi) lo = mid + 1u;
        else return 1;
    }
    return 0;
}

/* run = copy of base with room for `extra` more; false when the heap refuses (run untouched). */
static int run_from_base(sel_uim_selset* s, uint32_t extra) {
    uint32_t keep = s->run.n;
    s->run.n = 0;
    if (!runs_reserve(s, &s->run, s->base.n + extra)) {
        s->run.n = keep;
        return 0;
    }
    if (s->base.n != 0u) memcpy(s->run.v, s->base.v, (size_t)s->base.n * sizeof s->base.v[0]);
    s->run.n = s->base.n;
    return 1;
}

void sel_uim_selset_init(sel_uim_selset* s) {
    memset(s, 0, sizeof *s);
}

void sel_uim_selset_free(sel_uim_selset* s) {
    runs_release(&s->run);
    runs_release(&s->base);
    s->anchor = 0;
}

void sel_uim_selset_clear(sel_uim_selset* s) {
    s->run.n = 0;
    s->base.n = 0;
    s->anchor = 0;
}

int sel_uim_selset_bind(sel_uim_selset* s, sel_uim_list_id list) {
    int changed = s->list.kind != list.kind || s->list.id != list.id || s->list.version != list.version;
    if (changed) {
        sel_uim_selset_free(s);
        s->list = list;
    }
    return changed;
}

int sel_uim_selset_select_one(sel_uim_selset* s, uint32_t i) {
    if (!runs_reserve(s, &s->run, 1u)) return 0;
    s->run.n = 0;
    s->base.n = 0;
    runs_add(&s->run, i, i);
    s->anchor = i;
    return 1;
}

int sel_uim_selset_toggle(sel_uim_selset* s, uint32_t i) {
    if (!runs_reserve(s, &s->run, 2u) || !runs_reserve(s, &s->base, s->run.n + 2u)) return 0;
    if (runs_has(&s->run, i)) runs_remove(&s->run, i);
    else runs_add(&s->run, i, i);
    memcpy(s->base.v, s->run.v, (size_t)s->run.n * sizeof s->run.v[0]);
    s->base.n = s->run.n;
    s->anchor = i;
    return 1;
}

int sel_uim_selset_extend(sel_uim_selset* s, uint32_t i) {
    uint32_t lo, hi;
    if (s->run.n == 0u) return sel_uim_selset_select_one(s, i);
    lo = s->anchor < i ? s->anchor : i;
    hi = s->anchor < i ? i : s->anchor;
    if (!run_from_base(s, 1u)) return 0;
    runs_add(&s->run, lo, hi);
    return 1;
}

int sel_uim_selset_select_all(sel_uim_selset* s, uint32_t row_count) {
    if (row_count == 0u) {
        sel_uim_selset_clear(s);
        return 1;
    }
    if (!runs_reserve(s, &s->run, 1u)) return 0;
    s->run.n = 0;
    s->base.n = 0;
    runs_add(&s->run, 0, row_count - 1u);
    s->anchor = 0;
    return 1;
}

int sel_uim_selset_contains(const sel_uim_selset* s, uint32_t i) {
    return runs_has(&s->run, i);
}

uint32_t sel_uim_selset_count(const sel_uim_selset* s) {
    uint32_t i, n = 0;
    for (i = 0; i < s->run.n; i++) n += s->run.v[i].hi - s->run.v[i].lo + 1u;
    return n;
}

uint32_t sel_uim_selset_run_count(const sel_uim_selset* s) {
    return s->run.n;
}

size_t sel_uim_selset_format(const sel_uim_selset* s, char* out, size_t cap) {
    size_t len = 0;
    uint32_t i;
    if (cap != 0u) out[0] = '\0';
    for (i = 0; i < s->run.n; i++) {
        char tmp[32];
        int w = s->run.v[i].lo == s->run.v[i].hi
                    ? snprintf(tmp, sizeof tmp, "%s%u", i ? "," : "", (unsigned)s->run.v[i].lo)
                    : snprintf(tmp, sizeof tmp, "%s%u-%u", i ? "," : "", (unsigned)s->run.v[i].lo, (unsigned)s->run.v[i].hi);
        if (len + (size_t)w + 1u > cap) {
            if (cap != 0u) out[0] = '\0';
            return 0;
        }
        memcpy(out + len, tmp, (size_t)w + 1u);
        len += (size_t)w;
    }
    return len;
}

int sel_uim_selset_parse(sel_uim_selset* s, const char* text) {
    sel_uim_runs fresh;
    const char* p = text;
    uint32_t lo, hi, last_hi = 0;
    int rc;
    memset(&fresh, 0, sizeof fresh);
    while ((rc = sel_ranges_next(&p, &lo, &hi)) > 0) {
        if (fresh.n != 0u && lo <= last_hi + 1u) { /* not ascending, or touching the run before */
            rc = -1;
            break;
        }
        if (!runs_reserve(s, &fresh, 1u)) {
            rc = -1;
            break;
        }
        fresh.v[fresh.n].lo = lo;
        fresh.v[fresh.n].hi = hi;
        fresh.n++;
        last_hi = hi;
    }
    if (rc < 0 || fresh.n == 0u) {
        runs_release(&fresh);
        return 0;
    }
    runs_release(&s->run);
    runs_release(&s->base);
    s->run = fresh;
    if (runs_reserve(s, &s->base, fresh.n)) {
        memcpy(s->base.v, fresh.v, (size_t)fresh.n * sizeof fresh.v[0]);
        s->base.n = fresh.n;
    }
    s->anchor = fresh.v[fresh.n - 1u].hi;
    return 1;
}
