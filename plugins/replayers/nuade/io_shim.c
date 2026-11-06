/* NuadeIo for the nuade adapter. sel_nuade_io_none(): every lookup reports "not found" (songs that need no companion).
 * sel_nuade_io_sibling() (I710au): reads the files that sit in the song's own directory (or below it): sample sets
 * (smp.set, smp.*), pn.* and a samples directory. A name that leaves the song's directory is refused. */
#include "shim.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool io_exists(struct NuadeIoPrivate* self, const char* url) {
    (void)self;
    (void)url;
    return false;
}

static NuadeIoReadResult io_read(struct NuadeIoPrivate* self, const char* url) {
    NuadeIoReadResult r;
    (void)self;
    (void)url;
    r.data = NULL;
    r.size = 0;
    return r;
}

static void io_free(struct NuadeIoPrivate* self, void* memory) {
    (void)self;
    (void)memory;
}

static bool io_list_dir(struct NuadeIoPrivate* self, const char* url, NuadeIoDirVisitor visit, void* ctx) {
    (void)self;
    (void)url;
    (void)visit;
    (void)ctx;
    return false;
}

static const NuadeIo k_io = {NUADE_IO_API_VERSION, NULL, io_exists, io_read, io_free, io_list_dir};

const NuadeIo* sel_nuade_io_none(void) {
    return &k_io;
}

/* Is `url` inside the song's directory? The root keeps its trailing separator ("dir/", "DH0:Music/", "/" or "" for a
 * bare file name). The rest of the url must be a plain relative path: no "..", no empty component ("a//b" is the
 * parent on AmigaDOS), no volume or backslash. */
static bool inside_root(const SelNuadeSiblingIo* io, const char* url) {
    size_t rl;
    const char* p;
    if (url == NULL) {
        return false;
    }
    rl = strlen(io->root);
    if (strncmp(url, io->root, rl) != 0) {
        return false;
    }
    p = url + rl;
    if (rl == 0u && *p == '/') {
        return false;
    }
    while (*p != '\0') {
        const char* e = strchr(p, '/');
        size_t l = e != NULL ? (size_t)(e - p) : strlen(p);
        if (l == 0u || (l == 2u && p[0] == '.' && p[1] == '.')) {
            return false;
        }
        if (memchr(p, ':', l) != NULL || memchr(p, '\\', l) != NULL) {
            return false;
        }
        p += l + (e != NULL ? 1u : 0u);
    }
    return true;
}

static bool sib_exists(struct NuadeIoPrivate* self, const char* url) {
    struct stat st;
    return inside_root((const SelNuadeSiblingIo*)self, url) && stat(url, &st) == 0;
}

static NuadeIoReadResult sib_read(struct NuadeIoPrivate* self, const char* url) {
    const SelNuadeSiblingIo* io = (const SelNuadeSiblingIo*)self;
    NuadeIoReadResult r;
    FILE* f;
    long len;
    r.data = NULL;
    r.size = 0;
    if (!inside_root(io, url) || (f = fopen(url, "rb")) == NULL) {
        return r;
    }
    len = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
    if (len >= 0 && len <= 0x4000000L && fseek(f, 0, SEEK_SET) == 0) {
        /* one spare byte: an empty file still gets a non-null buffer */
        r.data = (uint8_t*)io->host->mem_alloc(io->host, (size_t)len + 1u, SEL_MEM_FAST);
        if (r.data != NULL && fread(r.data, 1, (size_t)len, f) != (size_t)len) {
            io->host->mem_free(io->host, r.data);
            r.data = NULL;
        }
        if (r.data != NULL) {
            r.size = (uint64_t)len;
        }
    }
    fclose(f);
    return r;
}

static void sib_free(struct NuadeIoPrivate* self, void* memory) {
    const SelNuadeSiblingIo* io = (const SelNuadeSiblingIo*)self;
    if (memory != NULL) {
        io->host->mem_free(io->host, memory);
    }
}

static bool sib_list_dir(struct NuadeIoPrivate* self, const char* url, NuadeIoDirVisitor visit, void* ctx) {
    const SelNuadeSiblingIo* io = (const SelNuadeSiblingIo*)self;
    size_t rl = strlen(io->root);
    const char* path = url;
    DIR* d;
    const struct dirent* e;
    if (url == NULL || visit == NULL) {
        return false;
    }
    /* the root itself, with or without its trailing separator; "" means the current directory */
    if (rl == 0u && url[0] == '\0') {
        path = ".";
    } else if (!(strcmp(url, io->root) == 0 || (rl > 1u && strncmp(url, io->root, rl - 1u) == 0 && url[rl - 1u] == '\0')) &&
               !inside_root(io, url)) {
        return false;
    }
    d = opendir(path);
    if (d == NULL) {
        return false;
    }
    while ((e = readdir(d)) != NULL) {
        char full[1100];
        struct stat st;
        int n;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        n = snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        visit(e->d_name, n > 0 && (size_t)n < sizeof full && stat(full, &st) == 0 && S_ISDIR(st.st_mode), ctx);
    }
    closedir(d);
    return true;
}

const NuadeIo* sel_nuade_io_sibling(SelNuadeSiblingIo* io, const sel_host* host, const char* song_path) {
    const char* cut = NULL;
    const char* p;
    size_t n;
    if (song_path == NULL) {
        return sel_nuade_io_none();
    }
    for (p = song_path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') {
            cut = p;
        }
    }
    n = cut != NULL ? (size_t)(cut - song_path) + 1u : 0u;
    if (n >= sizeof io->root) {
        return sel_nuade_io_none();
    }
    memcpy(io->root, song_path, n);
    io->root[n] = '\0';
    io->host = host;
    io->io.api_version = NUADE_IO_API_VERSION;
    io->io.private_data = (struct NuadeIoPrivate*)io;
    io->io.exists = sib_exists;
    io->io.read_url_to_memory = sib_read;
    io->io.free_url_to_memory = sib_free;
    io->io.list_dir = sib_list_dir;
    return &io->io;
}

/* nuade_song.c (NuadeSong, the framework's own song object and mixer driver)
 * is not linked: Selecta owns the song, the clock and the mixer. The only
 * references to it are the pt4_from_nuade accessors of the players, which a
 * NuadeSong-free host never calls; they get a "no song" answer. */
const NuadePlayer* nuade_player(const NuadeSong* song) {
    (void)song;
    return NULL;
}

void* nuade_player_data(NuadeSong* song) {
    (void)song;
    return NULL;
}
