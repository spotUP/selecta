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
