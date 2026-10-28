/* Song length database baker (I605); host build tool, format in songlen.h. Not part of selecta_library. */
#include "songlen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

typedef struct bake_entry {
    uint8_t key[6];
    uint8_t first, count;
    uint32_t offset;
} bake_entry;

static int cmp_entry(const void* a, const void* b) {
    const bake_entry* x = (const bake_entry*)a;
    const bake_entry* y = (const bake_entry*)b;
    int c = memcmp(x->key, y->key, 6);
    if (c != 0) return c;
    return x->offset < y->offset ? -1 : (x->offset > y->offset);
}

long sel_songlen_bake(const char* tsv_path, const char* out_path, long* out_bytes) {
    FILE* in = fopen(tsv_path, "rb");
    FILE* out;
    bake_entry* ents = NULL;
    uint32_t* lens = NULL;
    size_t n = 0, cap = 0, nl = 0, capl = 0, i, written = 0;
    char* line = NULL;
    size_t linecap = 0;
    long result = -1;
    int ch;
    size_t len = 0;
    uint8_t buf[SEL_SONGLEN_HEADER]; /* the header (16) is larger than an entry (12) */

    if (in == NULL) return -1;
    /* A hand-rolled line reader keeps this portable C99 (no getline). */
    for (;;) {
        ch = fgetc(in);
        if (ch == EOF && len == 0) break;
        if (ch == '\n' || ch == EOF) {
            char *tab1, *tab2, *p;
            uint8_t key[6];
            unsigned long first;
            size_t start = nl, cnt = 0;
            line[len] = '\0';
            if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
            tab1 = strchr(line, '\t');
            tab2 = tab1 != NULL ? strchr(tab1 + 1, '\t') : NULL;
            if (tab1 != NULL && tab2 != NULL && tab1 - line == 12) {
                line[12] = '\0';
                if (sel_songlen_key_bytes(line, key)) {
                    first = strtoul(tab1 + 1, NULL, 10);
                    p = tab2 + 1;
                    while (*p != '\0' && cnt < SEL_SONGLEN_MAX_SUBSONGS && first + cnt <= 255u) {
                        unsigned long v = strtoul(p, &p, 10);
                        while (*p != '\0' && *p != ' ') p++; /* ",flags" */
                        while (*p == ' ') p++;
                        if (nl == capl) {
                            uint32_t* g;
                            capl = capl != 0 ? capl * 2u : 1024u;
                            g = (uint32_t*)realloc(lens, capl * sizeof *lens);
                            if (g == NULL) goto done;
                            lens = g;
                        }
                        lens[nl++] = v > 0xffffffffUL ? 0u : (uint32_t)v;
                        cnt++;
                    }
                    if (cnt > 0) {
                        if (n == cap) {
                            bake_entry* g;
                            cap = cap != 0 ? cap * 2u : 1024u;
                            g = (bake_entry*)realloc(ents, cap * sizeof *ents);
                            if (g == NULL) goto done;
                            ents = g;
                        }
                        memcpy(ents[n].key, key, 6);
                        ents[n].first = (uint8_t)first;
                        ents[n].count = (uint8_t)cnt;
                        ents[n].offset = (uint32_t)start;
                        n++;
                    }
                }
            }
            len = 0;
            if (ch == EOF) break;
            continue;
        }
        if (len + 2 > linecap) {
            char* g;
            linecap = linecap != 0 ? linecap * 2u : 256u;
            g = (char*)realloc(line, linecap);
            if (g == NULL) goto done;
            line = g;
        }
        line[len++] = (char)ch;
    }
    qsort(ents, n, sizeof *ents, cmp_entry);
    /* Duplicate keys: the first row in file order (lowest offset) wins. */
    for (i = 0; i < n; i++) {
        if (written > 0 && memcmp(ents[written - 1].key, ents[i].key, 6) == 0) continue;
        ents[written++] = ents[i];
    }
    n = written;
    out = fopen(out_path, "wb");
    if (out == NULL) goto done;
    memcpy(buf, "SLDB", 4);
    put32(buf + 4, 1u);
    put32(buf + 8, (uint32_t)n);
    put32(buf + 12, (uint32_t)(SEL_SONGLEN_HEADER + n * SEL_SONGLEN_ENTRY));
    result = fwrite(buf, 1, SEL_SONGLEN_HEADER, out) == SEL_SONGLEN_HEADER ? (long)n : -1;
    for (i = 0; result >= 0 && i < n; i++) {
        memcpy(buf, ents[i].key, 6);
        buf[6] = ents[i].first;
        buf[7] = ents[i].count;
        put32(buf + 8, ents[i].offset);
        if (fwrite(buf, 1, SEL_SONGLEN_ENTRY, out) != SEL_SONGLEN_ENTRY) result = -1;
    }
    for (i = 0; result >= 0 && i < nl; i++) {
        put32(buf, lens[i]);
        if (fwrite(buf, 1, 4, out) != 4) result = -1;
    }
    if (fclose(out) != 0) result = -1;
    if (result >= 0 && out_bytes != NULL) *out_bytes = (long)(SEL_SONGLEN_HEADER + n * SEL_SONGLEN_ENTRY + nl * 4u);
done:
    fclose(in);
    free(ents);
    free(lens);
    free(line);
    return result;
}
