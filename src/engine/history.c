#include "history.h"

#include <string.h>

int sel_history_init(sel_history* h, const sel_host* host, uint32_t capacity, uint32_t text_bytes) {
    memset(h, 0, sizeof *h);
    h->capacity = capacity;
    h->text_bytes = text_bytes;
    h->ids = host->mem_alloc(host, (size_t)2u * SEL_HISTORY_DEPTH * capacity * sizeof *h->ids, SEL_MEM_FAST);
    h->dead = host->mem_alloc(host, (size_t)capacity * sizeof *h->dead, SEL_MEM_FAST);
    h->scratch = host->mem_alloc(host, (size_t)capacity * sizeof *h->scratch, SEL_MEM_FAST);
    h->dead_text = host->mem_alloc(host, text_bytes, SEL_MEM_FAST);
    h->scratch_text = host->mem_alloc(host, text_bytes, SEL_MEM_FAST);
    if (!h->ids || !h->dead || !h->scratch || !h->dead_text || !h->scratch_text) {
        sel_history_free(h, host);
        return 0;
    }
    return 1;
}

void sel_history_free(sel_history* h, const sel_host* host) {
    if (h->ids) {
        host->mem_free(host, h->ids);
    }
    if (h->dead) {
        host->mem_free(host, h->dead);
    }
    if (h->scratch) {
        host->mem_free(host, h->scratch);
    }
    if (h->dead_text) {
        host->mem_free(host, h->dead_text);
    }
    if (h->scratch_text) {
        host->mem_free(host, h->scratch_text);
    }
    memset(h, 0, sizeof *h);
}

static uint32_t* undo_slot(sel_history* h, uint32_t i) {
    return h->ids + (size_t)((h->undo_head + i) % SEL_HISTORY_DEPTH) * h->capacity;
}

static uint32_t* redo_slot(sel_history* h, uint32_t i) {
    return h->ids + (size_t)(SEL_HISTORY_DEPTH + i) * h->capacity;
}

static void snapshot(uint32_t* dst, const sel_queue* q) {
    uint32_t i;
    for (i = 0; i < q->count; i++) {
        dst[i] = q->entries[i].id;
    }
}

static void reset(sel_history* h) {
    h->undo_head = 0;
    h->undo_count = 0;
    h->redo_count = 0;
    h->dead_count = 0;
    h->dead_used = 0;
}

void sel_history_record(sel_history* h, const sel_queue* q) {
    uint32_t slot;
    if (h->undo_count == SEL_HISTORY_DEPTH) {
        h->undo_head = (h->undo_head + 1u) % SEL_HISTORY_DEPTH;
        h->undo_count--;
    }
    slot = h->undo_count++;
    snapshot(undo_slot(h, slot), q);
    h->undo_len[(h->undo_head + slot) % SEL_HISTORY_DEPTH] = q->count;
    h->redo_count = 0;
}

void sel_history_forget(sel_history* h) {
    if (h->undo_count > 0) {
        h->undo_count--;
    }
}

static const sel_queue_entry* dead_find(const sel_history* h, uint32_t id) {
    uint32_t i;
    for (i = 0; i < h->dead_count; i++) {
        if (h->dead[i].id == id) {
            return &h->dead[i];
        }
    }
    return NULL;
}

/* 0: the dead table is full. */
static int dead_add(sel_history* h, const sel_queue* q, uint32_t index) {
    const sel_queue_entry* en = &q->entries[index];
    const char* path = q->text + en->path_offset;
    size_t n = strlen(path) + 1u;
    if (dead_find(h, en->id) != NULL) {
        return 1;
    }
    if (h->dead_count >= h->capacity || n > h->text_bytes - h->dead_used) {
        return 0;
    }
    h->dead[h->dead_count] = *en;
    h->dead[h->dead_count].path_offset = h->dead_used;
    memcpy(h->dead_text + h->dead_used, path, n);
    h->dead_used += (uint32_t)n;
    h->dead_count++;
    return 1;
}

void sel_history_bury(sel_history* h, const sel_queue* q, uint32_t index) {
    uint32_t i = index == SEL_QUEUE_NONE ? 0 : index;
    uint32_t end = index == SEL_QUEUE_NONE ? q->count : index + 1u;
    for (; i < end; i++) {
        if (!dead_add(h, q, i)) {
            reset(h);
            return;
        }
    }
}

static int in_list(const uint32_t* ids, uint32_t n, uint32_t id) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }
    return 0;
}

/* Rewrites the queue to the order `target` (n ids). Entries that leave the
 * queue are buried first; entries that return come from the dead table. */
static int rewrite(sel_history* h, sel_queue* q, const uint32_t* target, uint32_t n) {
    uint32_t i, used = 0, out = 0;
    for (i = 0; i < q->count; i++) {
        if (!in_list(target, n, q->entries[i].id) && !dead_add(h, q, i)) {
            return 0;
        }
    }
    for (i = 0; i < n; i++) {
        uint32_t at = sel_queue_index_of(q, target[i]);
        const sel_queue_entry* src;
        const char* path;
        size_t len;
        if (at != SEL_QUEUE_NONE) {
            src = &q->entries[at];
            path = q->text + src->path_offset;
        } else {
            src = dead_find(h, target[i]);
            if (src == NULL) {
                continue;
            }
            path = h->dead_text + src->path_offset;
        }
        len = strlen(path) + 1u;
        h->scratch[out] = *src;
        h->scratch[out].path_offset = used;
        memcpy(h->scratch_text + used, path, len);
        used += (uint32_t)len;
        out++;
    }
    memcpy(q->entries, h->scratch, (size_t)out * sizeof *q->entries);
    memcpy(q->text, h->scratch_text, used);
    q->count = out;
    q->text_used = used;
    q->version++;
    return 1;
}

static int step(sel_history* h, sel_queue* q, int undo) {
    uint32_t n, from;
    uint32_t* src;
    uint32_t* dst;
    if (undo ? h->undo_count == 0 : h->redo_count == 0) {
        return 0;
    }
    if (undo) {
        from = h->undo_count - 1u;
        src = undo_slot(h, from);
        n = h->undo_len[(h->undo_head + from) % SEL_HISTORY_DEPTH];
        dst = redo_slot(h, h->redo_count);
        h->redo_len[h->redo_count] = q->count;
    } else {
        from = h->redo_count - 1u;
        src = redo_slot(h, from);
        n = h->redo_len[from];
        dst = undo_slot(h, h->undo_count);
        h->undo_len[(h->undo_head + h->undo_count) % SEL_HISTORY_DEPTH] = q->count;
    }
    /* the target is read from src while the current order goes to dst: different slots */
    snapshot(dst, q);
    if (!rewrite(h, q, src, n)) {
        reset(h);
        return 0;
    }
    if (undo) {
        h->undo_count--;
        h->redo_count++;
    } else {
        h->redo_count--;
        h->undo_count++;
    }
    return 1;
}

int sel_history_undo(sel_history* h, sel_queue* q) {
    return step(h, q, 1);
}

int sel_history_redo(sel_history* h, sel_queue* q) {
    return step(h, q, 0);
}
