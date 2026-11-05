#include "spsc.h"

#include "../core/count.h"
#include "../core/pal.h"

#include <selecta/tags.h>

#include <string.h>

/* Head/tail publications so far (sentinel spsc_publishes=). */
static uint32_t g_publishes;

uint32_t sel_spsc_publishes(void) {
    return g_publishes;
}

static uint32_t round_pow2(uint32_t n) {
    uint32_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

int sel_spsc_init_arena(sel_spsc* q, const sel_host* host, uint32_t capacity, uint32_t arena_bytes) {
    q->capacity = round_pow2(capacity < 2 ? 2 : capacity);
    q->head = q->tail = 0;
    q->arena = NULL;
    q->read = 0;
    q->arena_bytes = arena_bytes;
    q->slots = host->mem_alloc(host, (size_t)q->capacity * sizeof(sel_message), SEL_MEM_FAST);
    if (q->slots == NULL) {
        return 0;
    }
    if (arena_bytes != 0) {
        q->arena = host->mem_alloc(host, (size_t)q->capacity * arena_bytes, SEL_MEM_FAST);
        if (q->arena == NULL) {
            sel_spsc_free(q, host);
            return 0;
        }
    }
    return 1;
}

int sel_spsc_init(sel_spsc* q, const sel_host* host, uint32_t capacity) {
    return sel_spsc_init_arena(q, host, capacity, 0);
}

void sel_spsc_free(sel_spsc* q, const sel_host* host) {
    if (q->slots) {
        host->mem_free(host, q->slots);
        q->slots = NULL;
    }
    if (q->arena) {
        host->mem_free(host, q->arena);
        q->arena = NULL;
    }
}

uint32_t sel_spsc_count(const sel_spsc* q) {
    return SEL_ATOMIC_LOAD_ACQ(&q->tail) - q->read; /* consumer side: what is still to pop */
}

/* Copies every string argument of `src` (already in *dst) into `buf` of `cap` bytes and repoints dst at the copies.
 * 0 when they do not fit. */
/* Copies a tag list (MORE and SKIP followed) into buf at *used as a flat END-terminated array, string-typed values
 * included. PTR and INT values stay as they are (a PTR points into engine memory that outlives the event). */
static int copy_tags(sel_arg* dst, const sel_tag* src, char* buf, uint32_t cap, uint32_t* used) {
    uint32_t n = sel_tag_count(src), off = (*used + (uint32_t)sizeof(uintptr_t) - 1u) & ~((uint32_t)sizeof(uintptr_t) - 1u), i = 0, len;
    sel_tag* out;
    const sel_tag* it = src;
    const sel_tag* t;
    sel_tag_type ty;

    if (off > cap || (cap - off) / (uint32_t)sizeof(sel_tag) < n + 1u) {
        return 0;
    }
    out = (sel_tag*)(void*)(buf + off);
    *used = off + (n + 1u) * (uint32_t)sizeof(sel_tag);
    while ((t = sel_tag_next(&it)) != NULL && i < n) {
        out[i] = *t;
        if (t->value != 0 && sel_tag_type_of(t->tag, &ty) && ty == SEL_TT_STR) {
            len = (uint32_t)strlen((const char*)t->value) + 1u;
            if (len > cap - *used) {
                return 0;
            }
            memcpy(buf + *used, (const char*)t->value, len);
            out[i].value = (uintptr_t)(buf + *used);
            *used += len;
        }
        i++;
    }
    out[i].tag = SEL_TAG_END;
    out[i].value = 0;
    dst->v.t = out;
    return 1;
}

static int copy_strings(sel_message* dst, const sel_message* src, char* buf, uint32_t cap) {
    uint32_t used = 0, i, n;
    for (i = 0; i < src->argc && i < SEL_MAX_ARGS; i++) {
        if (src->args[i].type == SEL_ARG_STR && src->args[i].v.s != NULL) {
            n = (uint32_t)strlen(src->args[i].v.s) + 1u;
            if (n > cap - used) {
                return 0;
            }
            memcpy(buf + used, src->args[i].v.s, n);
            dst->args[i].v.s = buf + used;
            used += n;
        } else if (src->args[i].type == SEL_ARG_TAGS && src->args[i].v.t != NULL &&
                   !copy_tags(&dst->args[i], src->args[i].v.t, buf, cap, &used)) {
            return 0;
        }
    }
    return 1;
}

int sel_spsc_push(sel_spsc* q, const sel_message* m) {
    uint32_t tail = q->tail; /* the one producer owns tail */
    sel_message* slot;
    if (tail - SEL_ATOMIC_LOAD_ACQ(&q->head) >= q->capacity) {
        return 0;
    }
    slot = &q->slots[tail & (q->capacity - 1)];
    *slot = *m;
    if (q->arena_bytes != 0 && !copy_strings(slot, m, q->arena + (size_t)(tail & (q->capacity - 1)) * q->arena_bytes, q->arena_bytes)) {
        return 0;
    }
    sel_pal_publish_begin(); /* only the index store is guarded; the slot copy above is not */
    SEL_ATOMIC_STORE_REL(&q->tail, tail + 1u);
    g_publishes++;
    sel_pal_publish_end();
    return 1;
}

int sel_spsc_pop(sel_spsc* q, sel_message* out) {
    uint32_t read = q->read; /* the one consumer owns read */
    if (SEL_ATOMIC_LOAD_ACQ(&q->tail) == read) {
        return 0;
    }
    *out = q->slots[read & (q->capacity - 1)];
    if (q->arena_bytes != 0) {
        /* strings already point into the slot's arena (push repointed them); the slot is returned by release */
        q->read = read + 1u;
        return 1;
    }
    sel_pal_publish_begin();
    q->read = read + 1u;
    SEL_ATOMIC_STORE_REL(&q->head, read + 1u);
    g_publishes++;
    sel_pal_publish_end();
    return 1;
}

void sel_spsc_release(sel_spsc* q) {
    if (q->arena_bytes != 0 && q->head != q->read) {
        sel_pal_publish_begin();
        SEL_ATOMIC_STORE_REL(&q->head, q->read);
        g_publishes++;
        sel_pal_publish_end();
    }
}
