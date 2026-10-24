/* Animation timeline (DUI-P3-5). Integers only; tables built at init from integer formulas. */
#include <selecta/gui/gui_anim.h>

#include <string.h>

#define ONE 65536u

void sel_gui_timeline_init(sel_gui_timeline* t) {
    uint32_t i;
    memset(t, 0, sizeof *t);
    for (i = 0; i < SEL_GUI_EASE_ENTRIES; i++) {
        uint32_t x = (uint32_t)(((uint64_t)i * ONE) / (SEL_GUI_EASE_ENTRIES - 1u));
        uint32_t inv = ONE - x;
        t->table[SEL_GUI_EASE_LINEAR][i] = x;
        t->table[SEL_GUI_EASE_OUT][i] = ONE - (uint32_t)(((uint64_t)inv * inv) >> 16); /* quadratic out */
        if (x < ONE / 2u) { /* quadratic in-out */
            t->table[SEL_GUI_EASE_IN_OUT][i] = (uint32_t)(((uint64_t)x * x * 2u) >> 16);
        } else {
            t->table[SEL_GUI_EASE_IN_OUT][i] = ONE - (uint32_t)(((uint64_t)inv * inv * 2u) >> 16);
        }
    }
}

uint32_t sel_gui_ease_at(const sel_gui_timeline* t, uint32_t ease, uint32_t pos) {
    uint32_t scaled, i, frac, a, b;
    if (ease >= SEL_GUI_EASE_COUNT) {
        ease = SEL_GUI_EASE_LINEAR;
    }
    if (pos >= ONE) {
        return ONE;
    }
    scaled = pos * (SEL_GUI_EASE_ENTRIES - 1u);
    i = scaled >> 16;
    frac = scaled & 0xFFFFu;
    a = t->table[ease][i];
    b = t->table[ease][i + 1u];
    return a + (uint32_t)(((uint64_t)(b - a) * frac) >> 16);
}

static int32_t lerp(int32_t from, int32_t to, uint32_t k) {
    return from + (int32_t)(((int64_t)(to - from) * (int64_t)k) >> 16);
}

int sel_gui_tween_start(sel_gui_timeline* t, uint32_t key, int32_t from, int32_t to, uint32_t duration_ms,
                        uint32_t ease) {
    uint32_t i;
    sel_gui_tween* w = NULL;
    for (i = 0; i < SEL_GUI_TWEENS_MAX; i++) {
        if (t->tween[i].active && t->tween[i].key == key) {
            w = &t->tween[i];
            break;
        }
    }
    for (i = 0; w == NULL && i < SEL_GUI_TWEENS_MAX; i++) {
        if (!t->tween[i].active) {
            w = &t->tween[i];
        }
    }
    if (w == NULL || duration_ms == 0u) {
        if (w != NULL) {
            w->active = 0;
        }
        return 0;
    }
    w->key = key;
    w->from = from;
    w->to = to;
    w->value = from;
    w->start_ms = t->now_ms;
    w->duration_ms = duration_ms;
    w->ease = ease;
    w->active = 1;
    t->started++;
    return 1;
}

void sel_gui_timeline_advance(sel_gui_timeline* t, uint32_t now_ms) {
    uint32_t i;
    if (now_ms > t->now_ms) {
        t->now_ms = now_ms;
    }
    for (i = 0; i < SEL_GUI_TWEENS_MAX; i++) {
        sel_gui_tween* w = &t->tween[i];
        uint32_t el;
        if (!w->active) {
            continue;
        }
        el = t->now_ms - w->start_ms;
        if (el >= w->duration_ms) {
            w->value = w->to;
            w->active = 0;
        } else {
            w->value = lerp(w->from, w->to,
                            sel_gui_ease_at(t, w->ease, (uint32_t)(((uint64_t)el << 16) / w->duration_ms)));
        }
    }
}

int32_t sel_gui_tween_value(const sel_gui_timeline* t, uint32_t key, int32_t def) {
    uint32_t i;
    for (i = 0; i < SEL_GUI_TWEENS_MAX; i++) {
        if (t->tween[i].active && t->tween[i].key == key) {
            return t->tween[i].value;
        }
    }
    return def;
}

uint32_t sel_gui_timeline_running(const sel_gui_timeline* t) {
    uint32_t i, n = 0;
    for (i = 0; i < SEL_GUI_TWEENS_MAX; i++) {
        n += t->tween[i].active ? 1u : 0u;
    }
    return n;
}

void sel_gui_timeline_finish(sel_gui_timeline* t) {
    uint32_t i;
    for (i = 0; i < SEL_GUI_TWEENS_MAX; i++) {
        t->tween[i].active = 0;
    }
}
