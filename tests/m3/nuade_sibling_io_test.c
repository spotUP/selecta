/* I710au: the nuade adapter's companion-file IO reads the song's own directory and refuses everything else.
 * Drives sel_nuade_io_sibling() with a fake host: a sibling and a file in a subdirectory are read, and "../x",
 * an absolute path elsewhere, "a//b", a backslash and a volume prefix are refused (exists, read and list_dir). */
#include "../../plugins/replayers/nuade/shim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int allocs;
static void* h_alloc(const sel_host* h, size_t n, uint32_t k) {
    (void)h;
    (void)k;
    allocs++;
    return malloc(n);
}
static void h_free(const sel_host* h, void* p) {
    (void)h;
    allocs--;
    free(p);
}

static int failures;
#define EXPECT(c, msg) do { if (!(c)) { printf("FAIL %s\n", msg); failures++; } } while (0)

static void put(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        printf("FAIL cannot write %s\n", path);
        exit(1);
    }
    fputs(text, f);
    fclose(f);
}

static void count(const char* name, bool is_dir, void* ctx) {
    (void)name;
    (void)is_dir;
    (*(int*)ctx)++;
}

int main(void) {
    sel_host host;
    SelNuadeSiblingIo sib;
    const NuadeIo* io;
    NuadeIoReadResult r;
    char song[256], url[300];
    int n = 0;
    memset(&host, 0, sizeof host);
    host.mem_alloc = h_alloc;
    host.mem_free = h_free;
    mkdir("ni_root", 0777);
    mkdir("ni_root/samples", 0777);
    put("ni_root/song.pap", "S");
    put("ni_root/smp.set", "SET");
    put("ni_root/samples/one", "ONE");
    put("ni_outside", "OUT");
    snprintf(song, sizeof song, "ni_root/song.pap");
    io = sel_nuade_io_sibling(&sib, &host, song);
    EXPECT(io == &sib.io, "sibling io is returned for a path with a directory");
    r = NuadeIo_read_url_to_memory(io, "ni_root/smp.set");
    EXPECT(r.data != NULL && r.size == 3u && memcmp(r.data, "SET", 3) == 0, "sibling smp.set is read");
    if (r.data != NULL) {
        NuadeIo_free_url_to_memory(io, r.data);
    }
    EXPECT(allocs == 0, "read buffer returned through free");
    EXPECT(NuadeIo_exists(io, "ni_root/samples/one"), "file in a subdirectory exists");
    EXPECT(NuadeIo_list_dir(io, "ni_root", count, &n) && n == 3, "song directory lists (song, smp.set, samples)");
    snprintf(url, sizeof url, "ni_root/../ni_outside");
    EXPECT(!NuadeIo_exists(io, url), "../x does not exist");
    r = NuadeIo_read_url_to_memory(io, url);
    EXPECT(r.data == NULL, "../x is not read");
    r = NuadeIo_read_url_to_memory(io, "ni_root/samples/../../ni_outside");
    EXPECT(r.data == NULL, "nested .. is not read");
    r = NuadeIo_read_url_to_memory(io, "ni_outside");
    EXPECT(r.data == NULL, "a file outside the song directory is not read");
    r = NuadeIo_read_url_to_memory(io, "/etc/hosts");
    EXPECT(r.data == NULL, "an absolute path elsewhere is not read");
    r = NuadeIo_read_url_to_memory(io, "ni_root//smp.set");
    EXPECT(r.data == NULL, "an empty component is not read");
    r = NuadeIo_read_url_to_memory(io, "ni_root/samples\\one");
    EXPECT(r.data == NULL, "a backslash is not read");
    n = 0;
    EXPECT(!NuadeIo_list_dir(io, "ni_root/..", count, &n) && n == 0, "listing the parent is refused");
    EXPECT(!NuadeIo_list_dir(io, "/", count, &n), "listing / is refused");
    EXPECT(sel_nuade_io_sibling(&sib, &host, NULL) == sel_nuade_io_none(), "no path falls back to the empty io");
    printf(failures == 0 ? "nuade_sibling_io_ok\n" : "nuade_sibling_io_failed\n");
    remove("ni_root/samples/one");
    rmdir("ni_root/samples");
    remove("ni_root/smp.set");
    remove("ni_root/song.pap");
    rmdir("ni_root");
    remove("ni_outside");
    return failures == 0 ? 0 : 1;
}
