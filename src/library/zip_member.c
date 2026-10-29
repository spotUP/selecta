/* See zip_member.h. */
#include "zip_member.h"

#include "net/http.h"

#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t le16(const unsigned char* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t le32(const unsigned char* p) {
    return le16(p) | le16(p + 2) << 16;
}

int sel_zip_member_find(const char* cd_path, const char* name, sel_zip_member* out) {
    FILE* f = fopen(cd_path, "rb");
    size_t want = strlen(name);
    unsigned char h[46];
    int found = 0;

    if (f == NULL) {
        return 0;
    }
    while (!found && fread(h, 1, sizeof h, f) == sizeof h && memcmp(h, "PK\x01\x02", 4) == 0) {
        uint32_t nlen = le16(h + 28), skip = le16(h + 30) + le16(h + 32);
        char buf[SEL_ZIP_NAME_MAX];

        if (nlen < sizeof buf) {
            if (fread(buf, 1, nlen, f) != nlen) {
                break;
            }
            if (nlen == want && memcmp(buf, name, nlen) == 0) {
                out->method = (uint16_t)le16(h + 10);
                out->csize = le32(h + 20);
                out->usize = le32(h + 24);
                out->local_offset = le32(h + 42);
                found = 1;
            }
        } else if (fseek(f, (long)nlen, SEEK_CUR) != 0) {
            break;
        }
        if (fseek(f, (long)skip, SEEK_CUR) != 0) {
            break;
        }
    }
    fclose(f);
    return found;
}

typedef struct mem_sink {
    unsigned char* buf;
    size_t cap, len;
} mem_sink;

static int on_body(void* ctx, const void* data, size_t n) {
    mem_sink* s = ctx;

    if (n > s->cap - s->len) {
        return 0;
    }
    memcpy(s->buf + s->len, data, n);
    s->len += n;
    return 1;
}

/* Range GET of [from, from+want-1] into buf; 1 when the server answered 206 with exactly `want` bytes. */
static int get_range(const char* url, unsigned long from, size_t want, unsigned char* buf, unsigned* requests) {
    sel_http_req req;
    sel_http_res res;
    mem_sink s;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.url = url;
    req.has_range = 1;
    req.range_from = from;
    req.range_to = (long)(from + want - 1ul);
    s.buf = buf;
    s.cap = want;
    s.len = 0;
    if (requests != NULL) {
        (*requests)++;
    }
    return sel_http_request(&req, on_body, &s, &res) == SEL_HTTP_OK && res.status == 206 && s.len == want;
}

int sel_zip_member_fetch(const char* zip_url, const sel_zip_member* m, const char* out_path, unsigned* requests) {
    unsigned char lh[30];
    unsigned char* packed = NULL;
    unsigned char* data = NULL;
    FILE* f;
    unsigned long start;
    int ok = 0;

    if (m->usize == 0 || m->usize > SEL_ZIP_MEMBER_MAX || m->csize == 0 || m->csize > SEL_ZIP_MEMBER_MAX ||
        (m->method != 0 && m->method != 8) || (m->method == 0 && m->csize != m->usize)) {
        return 0;
    }
    if (!get_range(zip_url, m->local_offset, sizeof lh, lh, requests) || memcmp(lh, "PK\x03\x04", 4) != 0) {
        return 0;
    }
    start = (unsigned long)m->local_offset + sizeof lh + le16(lh + 26) + le16(lh + 28);
    packed = malloc(m->csize);
    if (packed == NULL || !get_range(zip_url, start, m->csize, packed, requests)) {
        free(packed);
        return 0;
    }
    if (m->method == 0) {
        data = packed;
        packed = NULL;
    } else {
        data = malloc(m->usize);
        if (data == NULL || tinfl_decompress_mem_to_mem(data, m->usize, packed, m->csize, 0) != m->usize) {
            free(packed);
            free(data);
            return 0;
        }
    }
    f = fopen(out_path, "wb");
    if (f != NULL) {
        ok = fwrite(data, 1, m->usize, f) == m->usize;
        ok = fclose(f) == 0 && ok;
    }
    free(packed);
    free(data);
    return ok;
}
