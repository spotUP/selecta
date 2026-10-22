/* NuadeIo for songs that need no companion file (protracker4): every lookup
 * reports "not found". Host file access for companions is an M1 row. */
#include "shim.h"

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
