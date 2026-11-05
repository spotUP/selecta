/* Support-file downloader (I751). See support.h. */
#include "support.h"

#include "entropy.h"
#include "net/http.h"

#include <selecta/engine.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BEEPOLA_COMMIT "4145966ce0e6d96eeed57b522882915fbad23244"
#define BEEPOLA_URL "https://raw.githubusercontent.com/atsidaev/beepola/" BEEPOLA_COMMIT "/"
#define BEEPOLA_LIC "Amstrad (ROM), Shiru (drum samples), Jason C. Brooke (Savage tables); in Beepola's source, no licence that allows passing on"

/* id TAB file TAB url TAB size TAB sha256 TAB cut_off TAB cut_len TAB piece_sha256 TAB plugin TAB licence. */
static const char k_builtin[] =
    "organya-bank\tdefault.wdb\thttps://raw.githubusercontent.com/Strultz/organya.h/f7a086fc00f21de5e1c410d38d7412627b8d2cf0/default.wdb\t436212\t"
    "cd538281bbbc110a4ca1b9321b1452b46b39a24048fa760e1e09ca36bf0eec88\t0\t0\t-\torganya\t"
    "Strultz's organya.h repository; Cave Story's bank, free to download, not to pass on\n"
    "beepola-data\tzx48.rom\t" BEEPOLA_URL "p1d.img\t65536\taf58ece8b8b40197f337fe11ab8c75478c93d81ab5f0dcd5eaa6d111ca20f38c\t0\t16384\t"
    "d55daa439b673b0e3f5897f99ac37ecb45f974d1862b4dadb85dec34af99cb42\tbeepola\t" BEEPOLA_LIC "\n"
    "beepola-data\tp1d-samples.bin\t" BEEPOLA_URL "p1d.img\t65536\taf58ece8b8b40197f337fe11ab8c75478c93d81ab5f0dcd5eaa6d111ca20f38c\t33484\t1024\t"
    "552bef0cd0ad3890b542692248071489f91964f5c5aad3d3278a36b1760025ff\tbeepola\t" BEEPOLA_LIC "\n"
    "beepola-data\tsvg-table-tail.bin\t" BEEPOLA_URL "svg.img\t65536\t8a612d54a3519efe8754c0cdbf4d6405bd92e8d621d63e811a564e3d41805035\t33064\t136\t"
    "41ffa3729b07d6bd64370f93b880611cd8987d9ad257e81c0edddd18f8f231b5\tbeepola\t" BEEPOLA_LIC "\n";

static void copy(char* dst, size_t cap, const char* s, size_t n) {
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, s, n);
    dst[n] = '\0';
}

static int parse_line(const char* line, size_t len, sel_support_entry* e) {
    const char* f[10];
    size_t fl[10];
    unsigned i = 0;
    const char* p = line;
    const char* end = line + len;
    while (i < 10) {
        const char* t = memchr(p, '\t', (size_t)(end - p));
        if (t == NULL) {
            t = end;
        }
        f[i] = p;
        fl[i++] = (size_t)(t - p);
        if (t == end) {
            break;
        }
        p = t + 1;
    }
    if (i != 10 || fl[4] != 64) {
        return 0;
    }
    memset(e, 0, sizeof *e);
    copy(e->id, sizeof e->id, f[0], fl[0]);
    copy(e->file, sizeof e->file, f[1], fl[1]);
    copy(e->url, sizeof e->url, f[2], fl[2]);
    e->size = strtoul(f[3], NULL, 10);
    copy(e->sha256, sizeof e->sha256, f[4], fl[4]);
    e->cut_off = strtoul(f[5], NULL, 10);
    e->cut_len = strtoul(f[6], NULL, 10);
    if (fl[7] == 64) {
        copy(e->piece_sha256, sizeof e->piece_sha256, f[7], fl[7]);
    }
    copy(e->plugin, sizeof e->plugin, f[8], fl[8]);
    copy(e->licence, sizeof e->licence, f[9], fl[9]);
    /* the file name becomes a path: no separators, no dots-only names */
    return strchr(e->file, '/') == NULL && strchr(e->file, '\\') == NULL && e->file[0] != '.' && e->size > 0 &&
           (e->cut_len == 0 || (e->cut_off + e->cut_len <= e->size && e->piece_sha256[0] != '\0'));
}

static unsigned parse_text(const char* text, size_t n, sel_support_entry* out, unsigned cap) {
    unsigned count = 0;
    const char* p = text;
    const char* end = text + n;
    while (p < end && count < cap) {
        const char* nl = memchr(p, '\n', (size_t)(end - p));
        size_t len = nl != NULL ? (size_t)(nl - p) : (size_t)(end - p);
        if (len > 0 && p[0] != '#' && parse_line(p, len, &out[count])) {
            count++;
        }
        p += len + 1;
    }
    return count;
}

unsigned sel_support_manifest(sel_support_entry* out, unsigned cap) {
    const char* env = getenv("SELECTA_SUPPORT_MANIFEST");
    if (env != NULL && env[0] != '\0') {
        FILE* f = fopen(env, "rb");
        char* buf;
        long n;
        unsigned count = 0;
        if (f == NULL) {
            return 0;
        }
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc(n > 0 ? (size_t)n : 1u);
        if (buf != NULL && n > 0 && fread(buf, 1, (size_t)n, f) == (size_t)n) {
            count = parse_text(buf, (size_t)n, out, cap);
        }
        free(buf);
        fclose(f);
        return count;
    }
    return parse_text(k_builtin, sizeof k_builtin - 1, out, cap);
}

const char* sel_support_result_name(sel_support_result r) {
    static const char* const n[] = {"ok", "no_consent", "unknown", "fetch_failed", "hash_mismatch", "io_error"};
    return (unsigned)r < sizeof n / sizeof n[0] ? n[r] : "?";
}

static int hex_eq(const uint8_t d[32], const char* hex) {
    char h[65];
    unsigned i;
    for (i = 0; i < 32; i++) {
        snprintf(h + 2 * i, 3, "%02x", (unsigned)d[i]);
    }
    return strcmp(h, hex) == 0;
}

static int target(const char* cache_dir, const char* file, char* out, size_t cap) {
    int n = snprintf(out, cap, "%s/support/%s", cache_dir, file);
    return n > 0 && (size_t)n < cap;
}

static int file_ok(const char* path, const sel_support_entry* e) {
    FILE* f = fopen(path, "rb");
    unsigned long want = e->cut_len != 0 ? e->cut_len : e->size;
    uint8_t d[32];
    unsigned char* buf;
    int ok = 0;
    if (f == NULL) {
        return 0;
    }
    buf = malloc(want + 1);
    if (buf != NULL && fread(buf, 1, want + 1, f) == want) {
        sel_sha256(buf, want, d);
        ok = hex_eq(d, e->cut_len != 0 ? e->piece_sha256 : e->sha256);
    }
    free(buf);
    fclose(f);
    return ok;
}

int sel_support_path(const char* cache_dir, const char* file, char* out, size_t cap) {
    sel_support_entry m[SEL_SUPPORT_MAX];
    unsigned n = sel_support_manifest(m, SEL_SUPPORT_MAX), i;
    for (i = 0; i < n; i++) {
        if (strcmp(m[i].file, file) == 0) {
            return target(cache_dir, file, out, cap) && file_ok(out, &m[i]);
        }
    }
    return 0;
}

typedef struct body {
    unsigned char* data;
    size_t len, cap;
} body;

static int on_body(void* ctx, const void* data, size_t n) {
    body* b = ctx;
    if (b->len + n > b->cap) {
        return 0; /* longer than the manifest says */
    }
    memcpy(b->data + b->len, data, n);
    b->len += n;
    return 1;
}

sel_support_result sel_support_get(const char* cache_dir, const sel_support_entry* e, int consent) {
    char path[1100], part[1110], dir[1100];
    sel_http_req req;
    sel_http_res res;
    body b;
    uint8_t d[32];
    FILE* f;
    unsigned long want;
    sel_support_result r = SEL_SUPPORT_OK;
    if (e == NULL) {
        return SEL_SUPPORT_UNKNOWN;
    }
    if (!target(cache_dir, e->file, path, sizeof path)) {
        return SEL_SUPPORT_IO;
    }
    if (file_ok(path, e)) {
        return SEL_SUPPORT_OK; /* cached: no request, no consent needed to use it */
    }
    if (!consent) {
        return SEL_SUPPORT_NO_CONSENT;
    }
    b.data = malloc(e->size);
    b.len = 0;
    b.cap = e->size;
    if (b.data == NULL) {
        return SEL_SUPPORT_IO;
    }
    memset(&req, 0, sizeof req);
    req.url = e->url;
    req.total_ms = 60000;
    if (sel_http_request(&req, on_body, &b, &res) != SEL_HTTP_OK || res.status != 200 || b.len != e->size) {
        free(b.data);
        return SEL_SUPPORT_FETCH;
    }
    sel_sha256(b.data, b.len, d);
    if (!hex_eq(d, e->sha256)) {
        free(b.data);
        return SEL_SUPPORT_HASH;
    }
    want = e->cut_len != 0 ? e->cut_len : e->size;
    if (e->cut_len != 0) {
        sel_sha256(b.data + e->cut_off, want, d);
        if (!hex_eq(d, e->piece_sha256)) {
            free(b.data);
            return SEL_SUPPORT_HASH;
        }
    }
    snprintf(dir, sizeof dir, "%s/support", cache_dir);
    snprintf(part, sizeof part, "%s.part", path);
    if (!sel_config_mkdir(dir) || (f = fopen(part, "wb")) == NULL) {
        free(b.data);
        return SEL_SUPPORT_IO;
    }
    if (fwrite(b.data + e->cut_off, 1, want, f) != want) {
        r = SEL_SUPPORT_IO;
    }
    if (fclose(f) != 0 || (r == SEL_SUPPORT_OK && rename(part, path) != 0)) {
        r = SEL_SUPPORT_IO;
    }
    if (r != SEL_SUPPORT_OK) {
        remove(part);
    }
    free(b.data);
    return r;
}
