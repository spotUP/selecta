#include "queue.h"

#include <string.h>

void sel_queue_default_sizes(uint32_t fast_ram, uint32_t* capacity, uint32_t* text_bytes) {
    if (fast_ram >= 64u * 1024u * 1024u) {
        *capacity = 8192u;
        *text_bytes = 2u * 1024u * 1024u;
    } else if (fast_ram >= 8u * 1024u * 1024u) {
        *capacity = 2048u;
        *text_bytes = 256u * 1024u;
    } else {
        *capacity = 256u;
        *text_bytes = 32u * 1024u;
    }
}

int sel_queue_init(sel_queue* q, const sel_host* host, uint32_t capacity, uint32_t text_bytes) {
    memset(q, 0, sizeof *q);
    q->entries = host->mem_alloc(host, (size_t)capacity * sizeof *q->entries, SEL_MEM_FAST);
    q->text = host->mem_alloc(host, text_bytes, SEL_MEM_FAST);
    if (q->entries == NULL || q->text == NULL) {
        sel_queue_free(q, host);
        return 0;
    }
    q->capacity = capacity;
    q->text_bytes = text_bytes;
    q->next_id = 1;
    return 1;
}

void sel_queue_free(sel_queue* q, const sel_host* host) {
    if (q->entries) {
        host->mem_free(host, q->entries);
    }
    if (q->text) {
        host->mem_free(host, q->text);
    }
    memset(q, 0, sizeof *q);
}

int sel_queue_insert(sel_queue* q, uint32_t at, const char* path, uint32_t* id_out) {
    size_t n = strlen(path) + 1u;
    sel_queue_entry* en;
    if (at > q->count) {
        at = q->count;
    }
    if (q->count >= q->capacity || n > q->text_bytes - q->text_used) {
        return 0;
    }
    memmove(&q->entries[at + 1u], &q->entries[at], (size_t)(q->count - at) * sizeof *q->entries);
    en = &q->entries[at];
    en->id = q->next_id++;
    en->subsong_plus1 = 0;
    en->path_offset = q->text_used;
    memcpy(q->text + q->text_used, path, n);
    q->text_used += (uint32_t)n;
    q->count++;
    q->version++;
    *id_out = en->id;
    return 1;
}

uint32_t sel_queue_index_of(const sel_queue* q, uint32_t id) {
    uint32_t i;
    for (i = 0; i < q->count; i++) {
        if (q->entries[i].id == id) {
            return i;
        }
    }
    return SEL_QUEUE_NONE;
}

const char* sel_queue_path(const sel_queue* q, uint32_t index) {
    return q->text + q->entries[index].path_offset;
}

uint32_t sel_queue_remove(sel_queue* q, uint32_t id) {
    uint32_t at = sel_queue_index_of(q, id);
    uint32_t i, off, len;
    if (at == SEL_QUEUE_NONE) {
        return SEL_QUEUE_NONE;
    }
    off = q->entries[at].path_offset;
    len = (uint32_t)strlen(q->text + off) + 1u;
    /* close the hole in the arena, then shift the offsets that pointed past it */
    memmove(q->text + off, q->text + off + len, q->text_used - off - len);
    q->text_used -= len;
    memmove(&q->entries[at], &q->entries[at + 1u],
            (size_t)(q->count - at - 1u) * sizeof *q->entries);
    q->count--;
    for (i = 0; i < q->count; i++) {
        if (q->entries[i].path_offset > off) {
            q->entries[i].path_offset -= len;
        }
    }
    q->version++;
    return at;
}

void sel_queue_clear(sel_queue* q) {
    q->count = 0;
    q->text_used = 0;
    q->version++;
}
