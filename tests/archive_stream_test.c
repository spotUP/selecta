/* I161c: sel_archive_extract_file streams a zip member to a file. The zip is written here with miniz (an 8 MB
 * compressible member plus a decoy), the output must be byte-identical and miniz must never hold more than a
 * small fraction of the member on the host heap. */
#include "archive/archive.h"

#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMBER_BYTES (8u * 1024u * 1024u)
#define HEAP_BOUND (512u * 1024u)

static size_t write_cb(void* opaque, mz_uint64 ofs, const void* buf, size_t n) {
    (void)ofs;
    return fwrite(buf, 1, n, opaque);
}

static int fail(const char* what) {
    printf("FAIL %s\n", what);
    return 1;
}

int main(void) {
    const sel_host* host = sel_host_default();
    const char* zip_path = "archive_stream_test.zip";
    const char* out_path = "archive_stream_test.out";
    mz_zip_archive zip;
    uint8_t* data = malloc(MEMBER_BYTES);
    uint32_t bytes = 0, peak = 0;
    FILE* f;
    size_t i;

    if (data == NULL) {
        return fail("alloc");
    }
    for (i = 0; i < MEMBER_BYTES; i++) {
        data[i] = (uint8_t)((i % 251u) ^ (i / 4093u));
    }
    f = fopen(zip_path, "wb");
    memset(&zip, 0, sizeof zip);
    zip.m_pWrite = write_cb;
    zip.m_pIO_opaque = f;
    if (f == NULL || !mz_zip_writer_init(&zip, 0) ||
        !mz_zip_writer_add_mem(&zip, "dir/other.txt", "decoy", 5, MZ_BEST_SPEED) ||
        !mz_zip_writer_add_mem(&zip, "dir/allmods.txt", data, MEMBER_BYTES, MZ_BEST_SPEED) ||
        !mz_zip_writer_finalize_archive(&zip) || !mz_zip_writer_end(&zip)) {
        return fail("write zip");
    }
    fclose(f);
    if (!sel_archive_extract_file(host, zip_path, "allmods.txt", MEMBER_BYTES, out_path, &bytes, &peak)) {
        return fail("extract");
    }
    if (bytes != MEMBER_BYTES) {
        return fail("byte count");
    }
    if (peak == 0 || peak >= HEAP_BOUND) {
        printf("peak=%u\n", (unsigned)peak);
        return fail("peak heap not within (0, 512 KiB)");
    }
    f = fopen(out_path, "rb");
    for (i = 0; f != NULL && i < MEMBER_BYTES; i++) {
        if (fgetc(f) != data[i]) {
            return fail("content differs");
        }
    }
    if (f == NULL || fgetc(f) != EOF) {
        return fail("length differs");
    }
    fclose(f);
    if (sel_archive_extract_file(host, zip_path, "allmods.txt", MEMBER_BYTES - 1u, out_path, &bytes, &peak)) {
        return fail("max_member must reject");
    }
    if (sel_archive_extract_file(host, zip_path, "missing.txt", MEMBER_BYTES, out_path, &bytes, &peak)) {
        return fail("missing member must fail");
    }
    remove(zip_path);
    remove(out_path);
    free(data);
    printf("archive_stream_ok peak=%u\n", (unsigned)peak);
    return 0;
}
