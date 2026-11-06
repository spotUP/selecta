/* See sysfile.h. */
#include "sysfile.h"

#include "dir.h"
#include "entropy.h"
#include "zip_member.h"

#include <selecta/engine.h>

#if !defined(SEL_LIBRARY_NO_THREAD)
#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_N 16u

typedef struct cache_ent {
    char path[512];
    uint32_t size;
    int64_t mtime;
    int ok;
} cache_ent;

static cache_ent g_cache[CACHE_N];
static unsigned g_cache_next;
static unsigned g_hashes;

/* The Amiga build has one task (SEL_LIBRARY_NO_THREAD) and its pthread_mutex_t has no constant initializer that passes -Werror; no lock there, as vote_push.c. */
#if !defined(SEL_LIBRARY_NO_THREAD)
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#define SYSFILE_LOCK() pthread_mutex_lock(&g_lock)
#define SYSFILE_UNLOCK() pthread_mutex_unlock(&g_lock)
#else
#define SYSFILE_LOCK() ((void)0)
#define SYSFILE_UNLOCK() ((void)0)
#endif

unsigned sel_sysfile_hash_count(void) {
    unsigned n;

    SYSFILE_LOCK();
    n = g_hashes;
    SYSFILE_UNLOCK();
    return n;
}

/* 1 when the sha256 of data is one of the ';'-separated lowercase hex digests in `list`. */
static int sha_accepted(const char* list, const void* data, size_t n) {
    uint8_t d[32];
    char hex[65];
    unsigned i;
    const char* p;

    SYSFILE_LOCK();
    g_hashes++;
    SYSFILE_UNLOCK();
    sel_sha256(data, n, d);
    for (i = 0; i < 32; i++) {
        snprintf(hex + 2 * i, 3, "%02x", (unsigned)d[i]);
    }
    for (p = list; *p != '\0';) {
        size_t len = strcspn(p, ";");

        if (len == 64 && memcmp(p, hex, 64) == 0) {
            return 1;
        }
        p += len;
        if (*p == ';') {
            p++;
        }
    }
    return 0;
}

static int join(char* out, size_t cap, const char* dir, const char* name, const char* suffix) {
    int n = snprintf(out, cap, "%s/%s%s", dir, name, suffix);

    return n > 0 && (size_t)n < cap;
}

/* A plain system file is hashed whole; the 16 MB zip member cap does not apply (Mariopants' notes.wav is 25.7 MB, I710aa). */
#define SEL_SYSFILE_MAX_BYTES (64ul << 20)

static unsigned char* slurp(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    unsigned char* b = NULL;
    long len;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) >= 0 && (unsigned long)len <= SEL_SYSFILE_MAX_BYTES &&
        fseek(f, 0, SEEK_SET) == 0) {
        b = malloc(len > 0 ? (size_t)len : 1u);
        if (b != NULL && fread(b, 1, (size_t)len, f) != (size_t)len) {
            free(b);
            b = NULL;
        }
        *n = (size_t)len;
    }
    fclose(f);
    return b;
}

int sel_sysfile_resolve(const char* config_dir, const sel_sysfile_decl* d, char* path, size_t cap) {
    char full[512];
    uint32_t size = 0;
    int64_t mtime = 0;
    unsigned i;
    int ok = 0;

    if (config_dir == NULL || d == NULL || d->name == NULL || d->sha256 == NULL ||
        !join(full, sizeof full, config_dir, d->name, "") || !sel_file_stat(full, &size, &mtime) ||
        (d->size != 0 && size != d->size)) {
        return 0;
    }
    SYSFILE_LOCK();
    for (i = 0; i < CACHE_N; i++) {
        if (g_cache[i].path[0] != '\0' && strcmp(g_cache[i].path, full) == 0 && g_cache[i].size == size &&
            g_cache[i].mtime == mtime) {
            ok = g_cache[i].ok;
            SYSFILE_UNLOCK();
            goto done;
        }
    }
    SYSFILE_UNLOCK();
    {
        size_t n = 0;
        unsigned char* b = slurp(full, &n);
        cache_ent* c;

        ok = b != NULL && sha_accepted(d->sha256, b, n);
        free(b);
        SYSFILE_LOCK();
        c = &g_cache[g_cache_next++ % CACHE_N];
        snprintf(c->path, sizeof c->path, "%s", full);
        c->size = size;
        c->mtime = mtime;
        c->ok = ok;
        SYSFILE_UNLOCK();
    }
done:
    if (ok && path != NULL && cap > 0) {
        if (snprintf(path, cap, "%s", full) >= (int)cap) {
            path[0] = '\0';
        }
    }
    return ok;
}

typedef struct install_ctx {
    const sel_sysfile_decl* d;
    const char* part;
    const char* dest;
    int written;
} install_ctx;

/* Writes data to <name>.part and renames it over <name>. */
static int store(const char* part, const char* dest, const void* data, size_t n) {
    FILE* f = fopen(part, "wb");
    int ok = 0;

    if (f != NULL) {
        ok = fwrite(data, 1, n, f) == n;
        ok = fclose(f) == 0 && ok;
    }
    if (ok) {
        remove(dest); /* rename() does not replace on every platform */
        ok = rename(part, dest) == 0;
    }
    if (!ok) {
        remove(part);
    }
    return ok;
}

static int on_member(void* ctx, const char* name, const unsigned char* data, size_t n) {
    install_ctx* c = ctx;

    (void)name;
    if ((c->d->size != 0 && n != c->d->size) || !sha_accepted(c->d->sha256, data, n)) {
        return 0;
    }
    c->written = store(c->part, c->dest, data, n) ? 1 : -1;
    return 1;
}

static void reason(char* err, size_t cap, const char* fmt, const char* label) {
    if (err != NULL && cap > 0) {
        if (snprintf(err, cap, fmt, label) >= (int)cap) {
            err[cap - 1] = '\0';
        }
    }
}

int sel_sysfile_install(const char* config_dir, const sel_sysfile_decl* d, const char* src, char* err, size_t cap) {
    char part[512], dest[512];
    unsigned char magic[4] = {0, 0, 0, 0};
    const char* label = d != NULL && d->label != NULL ? d->label : "file";
    FILE* f;
    size_t got;
    int ok = 0;

    if (err != NULL && cap > 0) {
        err[0] = '\0';
    }
    if (config_dir == NULL || d == NULL || d->name == NULL || d->sha256 == NULL || src == NULL ||
        !join(part, sizeof part, config_dir, d->name, ".part") || !join(dest, sizeof dest, config_dir, d->name, "")) {
        reason(err, cap, "Invalid system file request for %s", label);
        return 0;
    }
    remove(part); /* a .part left by an earlier failed run is never used */
    f = fopen(src, "rb");
    if (f == NULL) {
        reason(err, cap, "Cannot read the file for %s", label);
        return 0;
    }
    got = fread(magic, 1, sizeof magic, f);
    fclose(f);
    if (!sel_config_mkdir(config_dir)) {
        reason(err, cap, "Cannot create the folder for %s", label);
        return 0;
    }
    if (got == 4 && memcmp(magic, "PK\x03\x04", 4) == 0) {
        install_ctx c;

        c.d = d;
        c.part = part;
        c.dest = dest;
        c.written = 0;
        if (!sel_zip_local_each(src, on_member, &c)) {
            reason(err, cap, "Cannot read this zip for %s", label);
        } else if (c.written == 0) {
            if (err != NULL && cap > 0 && snprintf(err, cap, "No file in this zip matches the %s", label) >= (int)cap) {
                err[cap - 1] = '\0';
            }
        } else if (c.written < 0) {
            reason(err, cap, "Cannot write %s", label);
        }
        ok = c.written > 0;
    } else {
        size_t n = 0;
        unsigned char* b = slurp(src, &n);

        if (b == NULL) {
            reason(err, cap, "Cannot read the file for %s", label);
        } else if ((d->size != 0 && n != d->size) || !sha_accepted(d->sha256, b, n)) {
            if (err != NULL && cap > 0 && snprintf(err, cap, "Not the %s", label) >= (int)cap) {
                err[cap - 1] = '\0';
            }
        } else if (!store(part, dest, b, n)) {
            reason(err, cap, "Cannot write %s", label);
        } else {
            ok = 1;
        }
        free(b);
    }
    return ok;
}
