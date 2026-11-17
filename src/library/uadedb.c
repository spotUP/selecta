/* uadeinfo.bin / uademd5.bin readers (I774k); format in uadedb.h. */
#include "uadedb.h"

#include "songlen.h"

#include <stdio.h>
#include <string.h>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t be24(const uint8_t* p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static int open_hdr(const char* path, const char* magic, FILE** f, uint8_t hdr[16]) {
    *f = path != NULL ? fopen(path, "rb") : NULL;
    if (*f == NULL) return 0;
    if (fread(hdr, 1, 16, *f) != 16 || memcmp(hdr, magic, 4) != 0 || be32(hdr + 4) != 1u) {
        fclose(*f);
        return 0;
    }
    return 1;
}

static void read_str(FILE* f, long at, char* dst, size_t cap) {
    size_t n;
    dst[0] = '\0';
    if (fseek(f, at, SEEK_SET) != 0) return;
    n = fread(dst, 1, cap - 1, f);
    dst[n] = '\0';
    /* fread fills the buffer past the NUL: strings are NUL-terminated in the file, so the first NUL ends it */
}

int sel_uadeinfo_get(const char* path, uint32_t entry, sel_uadeinfo* out) {
    FILE* f;
    uint8_t hdr[16], row[SEL_UADEINFO_ROW], b[4];
    uint32_t pool, nfmt, fmt;
    long strs;
    if (out == NULL || !open_hdr(path, "UINF", &f, hdr)) return 0;
    memset(out, 0, sizeof *out);
    pool = be32(hdr + 12);
    if (entry >= be32(hdr + 8) || fseek(f, (long)(16u + (uint64_t)entry * SEL_UADEINFO_ROW), SEEK_SET) != 0 ||
        fread(row, 1, sizeof row, f) != sizeof row || fseek(f, (long)pool, SEEK_SET) != 0 ||
        fread(b, 1, 4, f) != 4) {
        fclose(f);
        return 0;
    }
    nfmt = be32(b);
    strs = (long)pool + 4 + 4 * (long)nfmt;
    fmt = ((uint32_t)row[0] << 8) | row[1];
    out->channels = row[2];
    out->year = row[12] != 0 ? 1900u + row[12] : 0u;
    if (fmt < nfmt && fseek(f, (long)pool + 4 + 4 * (long)fmt, SEEK_SET) == 0 && fread(b, 1, 4, f) == 4)
        read_str(f, strs + (long)be32(b), out->format, sizeof out->format);
    read_str(f, strs + (long)be24(row + 3), out->authors, sizeof out->authors);
    read_str(f, strs + (long)be24(row + 6), out->publishers, sizeof out->publishers);
    read_str(f, strs + (long)be24(row + 9), out->album, sizeof out->album);
    fclose(f);
    return 1;
}

static const char* const k_credit_sources[SEL_UADECREDIT_SOURCES] = {
    "AMP", "Modland", "Demozoo", "Kestra", "UnExoticA", "ExoticA (old)", "Mods Anthology", "Fujiology", "Wanted Team"};

const char* sel_uadecredit_source_name(uint32_t source) { return source < SEL_UADECREDIT_SOURCES ? k_credit_sources[source] : ""; }

/* Opens `path`, reads the 16-byte header and the index words idx[entry], idx[entry + 1] (the row/id range). */
static int open_range(const char* path, const char* magic, uint32_t entry, FILE** f, uint32_t* pool, uint32_t* lo, uint32_t* hi) {
    uint8_t hdr[16], w[8];
    if (!open_hdr(path, magic, f, hdr)) return 0;
    *pool = be32(hdr + 12);
    if (entry >= be32(hdr + 8) || fseek(*f, (long)(16u + 4u * (uint64_t)entry), SEEK_SET) != 0 || fread(w, 1, 8, *f) != 8) {
        fclose(*f);
        return 0;
    }
    *lo = be32(w);
    *hi = be32(w + 4);
    return 1;
}

int sel_uadecredits_get(const char* path, uint32_t entry, sel_uadecredit* out, int max) {
    FILE* f;
    uint8_t hdr[16], row[SEL_UADECREDITS_ROW];
    uint32_t pool, lo, hi, n, i, cnt;
    long rows;
    int got = 0;
    if (out == NULL || max <= 0 || !open_range(path, "UCRD", entry, &f, &pool, &lo, &hi)) return 0;
    rewind(f);
    if (fread(hdr, 1, 16, f) != 16) {
        fclose(f);
        return 0;
    }
    n = be32(hdr + 8);
    rows = (long)(16u + 4u * (uint64_t)(n + 1u));
    cnt = hi - lo;
    for (i = 0; i < cnt && got < max; i++) {
        sel_uadecredit* c = &out[got];
        if (fseek(f, rows + (long)((uint64_t)(lo + i) * SEL_UADECREDITS_ROW), SEEK_SET) != 0 ||
            fread(row, 1, sizeof row, f) != sizeof row)
            break;
        memset(c, 0, sizeof *c);
        c->source = row[0];
        c->year = row[10] != 0 ? 1900u + row[10] : 0u;
        read_str(f, (long)pool + (long)be24(row + 1), c->authors, sizeof c->authors);
        read_str(f, (long)pool + (long)be24(row + 4), c->publishers, sizeof c->publishers);
        read_str(f, (long)pool + (long)be24(row + 7), c->album, sizeof c->album);
        got++;
    }
    fclose(f);
    return got;
}

int sel_uadeprov_get(const char* path, uint32_t entry, uint16_t* ids, int max) {
    FILE* f;
    uint8_t hdr[16], b[2];
    uint32_t pool, lo, hi, n, i;
    long base;
    int got = 0;
    if (ids == NULL || max <= 0 || !open_range(path, "UPRV", entry, &f, &pool, &lo, &hi)) return 0;
    rewind(f);
    if (fread(hdr, 1, 16, f) != 16) {
        fclose(f);
        return 0;
    }
    n = be32(hdr + 8);
    base = (long)(16u + 4u * (uint64_t)(n + 1u));
    if (fseek(f, base + 2 * (long)lo, SEEK_SET) == 0) {
        for (i = lo; i < hi && got < max && fread(b, 1, 2, f) == 2; i++) ids[got++] = (uint16_t)((b[0] << 8) | b[1]);
    }
    fclose(f);
    return got;
}

int sel_uadeprov_source(const char* path, uint32_t id, int* kind, char* file, size_t filecap, char* display, size_t dispcap) {
    FILE* f;
    uint8_t hdr[16], b[4];
    uint32_t i;
    int c;
    if (!open_hdr(path, "UPRV", &f, hdr)) return 0;
    if (fseek(f, (long)be32(hdr + 12), SEEK_SET) != 0 || fread(b, 1, 4, f) != 4 || id >= be32(b)) {
        fclose(f);
        return 0;
    }
    for (i = 0; i <= id; i++) { /* records are variable length: walk to the id */
        int k = fgetc(f);
        size_t fl = 0, dl = 0;
        if (k == EOF) { fclose(f); return 0; }
        if (i == id && kind != NULL) *kind = k;
        while ((c = fgetc(f)) != EOF && c != 0) {
            if (i == id && file != NULL && fl + 1 < filecap) file[fl++] = (char)c;
        }
        if (c == EOF) { fclose(f); return 0; }
        while ((c = fgetc(f)) != EOF && c != 0) {
            if (i == id && display != NULL && dl + 1 < dispcap) display[dl++] = (char)c;
        }
        if (c == EOF) { fclose(f); return 0; }
        if (i == id) {
            if (file != NULL && filecap > 0) file[fl] = '\0';
            if (display != NULL && dispcap > 0) display[dl] = '\0';
        }
    }
    fclose(f);
    return 1;
}

int sel_uademd5_lookup(const char* path, const char* md5hex, uint32_t* entry) {
    FILE* f;
    uint8_t hdr[16], want[6], row[SEL_UADEMD5_ROW];
    uint32_t lo = 0, hi;
    int found = 0;
    if (md5hex == NULL || !sel_songlen_key_bytes(md5hex, want) || !open_hdr(path, "UMD5", &f, hdr)) return 0;
    hi = be32(hdr + 8);
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int c;
        if (fseek(f, (long)(16u + (uint64_t)mid * SEL_UADEMD5_ROW), SEEK_SET) != 0 || fread(row, 1, sizeof row, f) != sizeof row) break;
        c = memcmp(want, row, 6);
        if (c < 0) {
            hi = mid;
        } else if (c > 0) {
            lo = mid + 1u;
        } else {
            if (entry != NULL) *entry = be24(row + 6);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}
