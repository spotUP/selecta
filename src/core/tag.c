#include <selecta/tags.h>

#include <string.h>

static const sel_tag_info k_tags[] = {
#define SEL_TAG(ns, name, value, type) {(value), #ns "_" #name, SEL_TT_##type},
#include <selecta/tags.def>
#undef SEL_TAG
};

#define SEL_TAG_TABLE_COUNT ((uint32_t)(sizeof(k_tags) / sizeof(k_tags[0])))

const sel_tag* sel_tag_next(const sel_tag** iter) {
    const sel_tag* t = *iter;
    for (;;) {
        if (t == NULL) {
            return NULL;
        }
        switch (t->tag) {
        case SEL_TAG_END:
            *iter = NULL;
            return NULL;
        case SEL_TAG_MORE:
            t = (const sel_tag*)(void*)t->value;
            break;
        case SEL_TAG_SKIP:
            t += 1 + t->value;
            break;
        default:
            *iter = t + 1;
            return t;
        }
    }
}

uintptr_t sel_tag_find(const sel_tag* list, uint32_t tag, uintptr_t def) {
    const sel_tag* it = list;
    const sel_tag* t;
    while ((t = sel_tag_next(&it)) != NULL) {
        if (t->tag == tag) {
            return t->value;
        }
    }
    return def;
}

uint32_t sel_tag_count(const sel_tag* list) {
    const sel_tag* it = list;
    uint32_t n = 0;
    while (sel_tag_next(&it) != NULL) {
        n++;
    }
    return n;
}

static const sel_tag_info* find_info(uint32_t tag) {
    uint32_t i;
    for (i = 0; i < SEL_TAG_TABLE_COUNT; i++) {
        if (k_tags[i].tag == tag) {
            return &k_tags[i];
        }
    }
    return NULL;
}

const char* sel_tag_name(uint32_t tag) {
    const sel_tag_info* info = find_info(tag);
    return info ? info->name : NULL;
}

int sel_tag_type_of(uint32_t tag, sel_tag_type* out) {
    const sel_tag_info* info = find_info(tag);
    if (info == NULL) {
        return 0;
    }
    if (out) {
        *out = info->type;
    }
    return 1;
}

int sel_tag_by_name(const char* name, uint32_t* out) {
    uint32_t i;
    for (i = 0; i < SEL_TAG_TABLE_COUNT; i++) {
        if (strcmp(k_tags[i].name, name) == 0) {
            if (out) {
                *out = k_tags[i].tag;
            }
            return 1;
        }
    }
    return 0;
}

const sel_tag_info* sel_tag_table(uint32_t* count) {
    if (count) {
        *count = SEL_TAG_TABLE_COUNT;
    }
    return k_tags;
}
