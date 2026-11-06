/* I710ga2: system-file store. Good raw file resolves, wrong sha is rejected with a reason, a member of a stored and a
 * deflated zip installs, a stale .part is never used, and the verdict cache (name, size, mtime) skips rehashing.
 * argv[1]: scratch directory. */
#include "entropy.h"
#include "sysfile.h"

#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            fails++;                                    \
        }                                               \
    } while (0)

static char g_dir[400];

static void path(char* out, size_t cap, const char* name) {
    snprintf(out, cap, "%s/%s", g_dir, name);
}

static void put(const char* name, const void* data, size_t n) {
    char p[512];
    FILE* f;

    path(p, sizeof p, name);
    f = fopen(p, "wb");
    if (f != NULL) {
        fwrite(data, 1, n, f);
        fclose(f);
    }
}

static int exists(const char* name) {
    char p[512];
    FILE* f;

    path(p, sizeof p, name);
    f = fopen(p, "rb");
    if (f != NULL) {
        fclose(f);
    }
    return f != NULL;
}

static void w16(unsigned char* p, unsigned v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void w32(unsigned char* p, unsigned long v) {
    w16(p, (unsigned)(v & 0xffffu));
    w16(p + 2, (unsigned)(v >> 16));
}

/* One-member zip, method 0 (stored) or 8 (deflate), written to file `name`. */
static void make_zip(const char* name, const char* member, const unsigned char* data, size_t n, int method) {
    unsigned char* z = calloc(1, n + 1024);
    unsigned char* packed = (unsigned char*)data;
    size_t pn = n, mlen = strlen(member), pos = 0, cd;
    unsigned long crc = mz_crc32(MZ_CRC32_INIT, data, n);
    void* heap = NULL;

    if (method == 8) {
        size_t out = 0;

        heap = tdefl_compress_mem_to_heap(data, n, &out, TDEFL_DEFAULT_MAX_PROBES);
        packed = heap;
        pn = out;
    }
    memcpy(z, "PK\x03\x04", 4);
    w16(z + 8, (unsigned)method);
    w32(z + 14, crc);
    w32(z + 18, pn);
    w32(z + 22, n);
    w16(z + 26, (unsigned)mlen);
    memcpy(z + 30, member, mlen);
    memcpy(z + 30 + mlen, packed, pn);
    pos = 30 + mlen + pn;
    cd = pos;
    memcpy(z + pos, "PK\x01\x02", 4);
    w16(z + pos + 10, (unsigned)method);
    w32(z + pos + 16, crc);
    w32(z + pos + 20, pn);
    w32(z + pos + 24, n);
    w16(z + pos + 28, (unsigned)mlen);
    memcpy(z + pos + 46, member, mlen);
    pos += 46 + mlen;
    memcpy(z + pos, "PK\x05\x06", 4);
    w16(z + pos + 8, 1);
    w16(z + pos + 10, 1);
    w32(z + pos + 12, pos - cd);
    w32(z + pos + 16, cd);
    pos += 22;
    put(name, z, pos);
    free(z);
    free(heap);
}

int main(int argc, char** argv) {
    unsigned char good[1024], bad[1024], other[1024];
    uint8_t d[32];
    char hex[65], full[512], src[512], err[200];
    sel_sysfile_decl decl;
    unsigned i, h0;

    if (argc < 2) {
        return 2;
    }
    snprintf(g_dir, sizeof g_dir, "%s/sysfile-store", argv[1]);
    {
        char cmd[600];

        snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir -p '%s'", g_dir, g_dir);
        if (system(cmd) != 0) {
            return 2;
        }
    }
    for (i = 0; i < sizeof good; i++) {
        good[i] = (unsigned char)(i * 7u + 3u);
        bad[i] = (unsigned char)(i * 5u + 1u);
        other[i] = (unsigned char)(i * 11u + 9u);
    }
    sel_sha256(good, sizeof good, d);
    for (i = 0; i < 32; i++) {
        snprintf(hex + 2 * i, 3, "%02x", (unsigned)d[i]);
    }
    memset(&decl, 0, sizeof decl);
    decl.plugin = "test";
    decl.name = "disk.d64";
    decl.label = "test disk image";
    decl.hint = "";
    decl.sha256 = hex;
    decl.urls = "";
    decl.size = sizeof good;

    /* absent, wrong bytes, right bytes */
    CHECK(!sel_sysfile_resolve(g_dir, &decl, full, sizeof full));
    put("disk.d64", bad, sizeof bad);
    CHECK(!sel_sysfile_resolve(g_dir, &decl, full, sizeof full));
    put("disk.d64", good, sizeof good);
    CHECK(sel_sysfile_resolve(g_dir, &decl, full, sizeof full));
    CHECK(strstr(full, "disk.d64") != NULL);

    /* size pre-check: a wrong declared size refuses without hashing */
    {
        sel_sysfile_decl s = decl;

        s.size = 999;
        h0 = sel_sysfile_hash_count();
        CHECK(!sel_sysfile_resolve(g_dir, &s, NULL, 0));
        CHECK(sel_sysfile_hash_count() == h0);
    }

    /* cache: an unchanged file is not hashed again */
    h0 = sel_sysfile_hash_count();
    for (i = 0; i < 5; i++) {
        CHECK(sel_sysfile_resolve(g_dir, &decl, NULL, 0));
    }
    CHECK(sel_sysfile_hash_count() == h0);
    /* a changed file (same size, other bytes) is hashed again and refused */
    put("disk.d64", bad, sizeof bad);
    {
        char p[512];
        char cmd[700];

        path(p, sizeof p, "disk.d64");
        snprintf(cmd, sizeof cmd, "touch -t 200001010000 '%s'", p); /* mtime moves even within one second */
        CHECK(system(cmd) == 0);
    }
    CHECK(!sel_sysfile_resolve(g_dir, &decl, NULL, 0));
    CHECK(sel_sysfile_hash_count() == h0 + 1);

    /* install: wrong raw file is rejected with a reason and nothing is stored */
    remove(full);
    path(src, sizeof src, "in.bin");
    put("in.bin", bad, sizeof bad);
    CHECK(!sel_sysfile_install(g_dir, &decl, src, err, sizeof err));
    CHECK(strstr(err, "Not the test disk image") != NULL);
    CHECK(!exists("disk.d64") && !exists("disk.d64.part"));

    /* install: right raw file */
    put("in.bin", good, sizeof good);
    CHECK(sel_sysfile_install(g_dir, &decl, src, err, sizeof err));
    CHECK(exists("disk.d64") && !exists("disk.d64.part"));
    CHECK(sel_sysfile_resolve(g_dir, &decl, NULL, 0));
    remove(full);

    /* a stale .part is never used and is removed by the next install */
    put("disk.d64.part", good, sizeof good);
    CHECK(!sel_sysfile_resolve(g_dir, &decl, NULL, 0));
    put("in.bin", bad, sizeof bad);
    CHECK(!sel_sysfile_install(g_dir, &decl, src, err, sizeof err));
    CHECK(!exists("disk.d64.part") && !exists("disk.d64"));

    /* install from a stored zip and a deflated zip, the matching member is not the first */
    for (i = 0; i < 2; i++) {
        make_zip("in.zip", "other.bin", other, sizeof other, i == 0 ? 0 : 8);
        CHECK(!sel_sysfile_install(g_dir, &decl, src, err, sizeof err)); /* in.bin is not a zip, wrong bytes */
        path(src, sizeof src, "in.zip");
        CHECK(!sel_sysfile_install(g_dir, &decl, src, err, sizeof err));
        CHECK(strstr(err, "No file in this zip matches the test disk image") != NULL);
        CHECK(!exists("disk.d64"));
        make_zip("in.zip", "dir/disk-any-name.d64", good, sizeof good, i == 0 ? 0 : 8);
        CHECK(sel_sysfile_install(g_dir, &decl, src, err, sizeof err));
        CHECK(exists("disk.d64") && !exists("disk.d64.part"));
        CHECK(sel_sysfile_resolve(g_dir, &decl, NULL, 0));
        remove(full);
        path(src, sizeof src, "in.bin");
    }

    /* I710aa: a plain file above the 16 MB zip member cap (Mariopants notes.wav is 25.7 MB) still resolves by sha. */
    {
        size_t big = 24u << 20;
        unsigned char* b = malloc(big);
        uint8_t bd[32];
        char bhex[65];
        sel_sysfile_decl bdecl = decl;

        CHECK(b != NULL);
        if (b != NULL) {
            for (i = 0; i < big; i++) b[i] = (unsigned char)(i * 13u + (i >> 9));
            sel_sha256(b, big, bd);
            for (i = 0; i < 32; i++) snprintf(bhex + 2 * i, 3, "%02x", (unsigned)bd[i]);
            bdecl.name = "big.wav";
            bdecl.sha256 = bhex;
            bdecl.size = (uint32_t)big;
            put("big.wav", b, big);
            CHECK(sel_sysfile_resolve(g_dir, &bdecl, NULL, 0));
            free(b);
        }
    }

    if (fails == 0) {
        printf("sysfile_store_ok\n");
    }
    return fails != 0;
}
