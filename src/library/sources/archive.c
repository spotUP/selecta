/* Archive adapters (plan Phase 4, I159; D-M3-7): zip via miniz, lha via lhasa. See archive.h. */
#include "archive.h"

#include <lha_reader.h>
#include <miniz.h>
#include <stdio.h>
#include <string.h>

#define MEMBER_NAME_MAX 512u

int sel_archive_kind(const uint8_t* data, size_t size) {
    if (size >= 4 && data[0] == 'P' && data[1] == 'K' && data[2] == 3 && data[3] == 4) {
        return SEL_ARCHIVE_ZIP;
    }
    /* LHA level 0..3 headers carry the method id "-lh?-" or "-lz?-" at offset 2. */
    if (size >= 7 && data[2] == '-' && data[3] == 'l' && (data[4] == 'h' || data[4] == 'z') && data[6] == '-') {
        return SEL_ARCHIVE_LHA;
    }
    if (size >= 10 && data[0] == 'L' && data[1] == 'Z' && data[2] == 'X') {
        return SEL_ARCHIVE_LZX;
    }
    if (size >= 16 && data[0] == 'P' && data[1] == 'P' && data[2] == '2' && data[3] == '0') {
        return SEL_ARCHIVE_PP20;
    }
    return SEL_ARCHIVE_NONE;
}

int sel_archive_is_folder(int kind) {
    return kind == SEL_ARCHIVE_ZIP || kind == SEL_ARCHIVE_LHA || kind == SEL_ARCHIVE_LZX;
}

static int each_zip(const sel_host* host, const uint8_t* data, size_t size, size_t max_member, int list_only,
                    sel_archive_member_fn fn, void* ctx) {
    mz_zip_archive zip;
    mz_uint i, n;
    int delivered = 0;

    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_mem(&zip, data, size, 0)) {
        return -1;
    }
    n = mz_zip_reader_get_num_files(&zip);
    for (i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        uint8_t* buf;
        int cont;

        if (mz_zip_reader_is_file_a_directory(&zip, i) || !mz_zip_reader_file_stat(&zip, i, &st) ||
            st.m_uncomp_size == 0 || st.m_uncomp_size > max_member) {
            continue;
        }
        if (list_only) {
            delivered++;
            if (!fn(ctx, st.m_filename, NULL, (size_t)st.m_uncomp_size)) {
                break;
            }
            continue;
        }
        buf = host->mem_alloc(host, (size_t)st.m_uncomp_size, SEL_MEM_FAST);
        if (buf == NULL) {
            continue;
        }
        if (!mz_zip_reader_extract_to_mem(&zip, i, buf, (size_t)st.m_uncomp_size, 0)) {
            host->mem_free(host, buf);
            continue;
        }
        cont = fn(ctx, st.m_filename, buf, (size_t)st.m_uncomp_size);
        host->mem_free(host, buf);
        delivered++;
        if (!cont) {
            break;
        }
    }
    mz_zip_reader_end(&zip);
    return delivered;
}

typedef struct mem_stream {
    const uint8_t* data;
    size_t size, pos;
} mem_stream;

static int mem_read(void* handle, void* buf, size_t len) {
    mem_stream* m = handle;
    size_t n = m->size - m->pos;

    if (n > len) {
        n = len;
    }
    memcpy(buf, m->data + m->pos, n);
    m->pos += n;
    return (int)n;
}

static int mem_skip(void* handle, size_t bytes) {
    mem_stream* m = handle;

    if (bytes > m->size - m->pos) {
        return 0;
    }
    m->pos += bytes;
    return 1;
}

static const LHAInputStreamType g_mem_type = {mem_read, mem_skip, NULL};

static int each_lha(const sel_host* host, const uint8_t* data, size_t size, size_t max_member, int list_only,
                    sel_archive_member_fn fn, void* ctx) {
    mem_stream ms;
    LHAInputStream* in;
    LHAReader* rd;
    LHAFileHeader* h;
    int delivered = 0;

    ms.data = data;
    ms.size = size;
    ms.pos = 0;
    in = lha_input_stream_new(&g_mem_type, &ms);
    if (in == NULL) {
        return -1;
    }
    rd = lha_reader_new(in);
    if (rd == NULL) {
        return -1;
    }
    while ((h = lha_reader_next_file(rd)) != NULL) {
        char name[MEMBER_NAME_MAX];
        uint8_t* buf;
        size_t got;
        int cont;

        if (h->filename == NULL || strcmp(h->compress_method, LHA_COMPRESS_TYPE_DIR) == 0 || h->length == 0 ||
            h->length > max_member) {
            continue;
        }
        snprintf(name, sizeof name, "%s%s", h->path != NULL ? h->path : "", h->filename);
        if (list_only) {
            delivered++;
            if (!fn(ctx, name, NULL, (size_t)h->length)) {
                break;
            }
            continue;
        }
        buf = host->mem_alloc(host, (size_t)h->length, SEL_MEM_FAST);
        if (buf == NULL) {
            continue;
        }
        got = lha_reader_read(rd, buf, (size_t)h->length);
        if (got != (size_t)h->length) {
            host->mem_free(host, buf);
            continue;
        }
        cont = fn(ctx, name, buf, got);
        host->mem_free(host, buf);
        delivered++;
        if (!cont) {
            break;
        }
    }
    lha_reader_free(rd);
    return delivered;
}

static int walk(const sel_host* host, int kind, const uint8_t* data, size_t size, size_t max_member,
                int list_only, sel_archive_member_fn fn, void* ctx) {
    if (kind == SEL_ARCHIVE_ZIP) {
        return each_zip(host, data, size, max_member, list_only, fn, ctx);
    }
    if (kind == SEL_ARCHIVE_LHA) {
        return each_lha(host, data, size, max_member, list_only, fn, ctx);
    }
    if (kind == SEL_ARCHIVE_LZX) {
        return sel_lzx_walk(host, data, size, max_member, list_only, fn, ctx);
    }
    return -1;
}

int sel_archive_each(const sel_host* host, int kind, const uint8_t* data, size_t size, size_t max_member,
                     sel_archive_member_fn fn, void* ctx) {
    return walk(host, kind, data, size, max_member, 0, fn, ctx);
}

int sel_archive_list(const sel_host* host, int kind, const uint8_t* data, size_t size,
                     sel_archive_member_fn fn, void* ctx) {
    return walk(host, kind, data, size, (size_t)-1, 1, fn, ctx);
}
