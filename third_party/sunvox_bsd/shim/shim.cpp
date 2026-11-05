/* Selecta shim: the few SunDog engine services the SunVox engine calls, written fresh (the SunDog window manager,
 * sound output and OS file system are not part of this build). Single engine instance, single thread. */
#include "core/core.h"
#include "memory/memory.h"
#include "time/timemanager.h"
#include "utils/utils.h"
#include "filesystem/v3nus_fs.h"
#include "sound/sound.h"
#include "core/debug.h"
#include <stdlib.h>
#include <string.h>

#define HDR 16u /* keeps 8-byte alignment; the size sits in the first word */

void* mem_new(unsigned long, unsigned long size, const UTF8_CHAR*, unsigned long) {
    unsigned char* p = (unsigned char*)calloc(1, size + HDR);
    if (p == NULL) return NULL;
    *(unsigned long*)p = size;
    return p + HDR;
}
void mem_free(void* ptr) { if (ptr) free((unsigned char*)ptr - HDR); }
void mem_set(void* ptr, unsigned long size, unsigned char v) { memset(ptr, v, size); }
void* mem_resize(void* ptr, int size) {
    unsigned char* p;
    unsigned long old;
    if (ptr == NULL) return mem_new(0, (unsigned long)size, NULL, 0);
    old = *(unsigned long*)((unsigned char*)ptr - HDR);
    p = (unsigned char*)realloc((unsigned char*)ptr - HDR, (size_t)size + HDR);
    if (p == NULL) return NULL;
    *(unsigned long*)p = (unsigned long)size;
    if ((unsigned long)size > old) memset(p + HDR + old, 0, (unsigned long)size - old);
    return p + HDR;
}
void mem_copy(void* d, const void* s, unsigned long n) { memcpy(d, s, n); }
int mem_cmp(const char* a, const char* b, unsigned long n) { return memcmp(a, b, n); }
void mem_strcat(char* d, const char* s) { strcat(d, s); }
long mem_strcmp(const char* a, const char* b) { return strcmp(a, b); }
int mem_strlen(const char* s) { return (int)strlen(s); }
char* mem_strdup(const char* s) {
    size_t n = strlen(s) + 1u;
    char* r = (char*)mem_new(0, n, NULL, 0);
    if (r) memcpy(r, s, n);
    return r;
}
long mem_get_size(void* ptr) { return ptr ? (long)*(unsigned long*)((unsigned char*)ptr - HDR) : 0; }
void mem_on(void) {}
void mem_off(void) {}

static ticks_t g_ticks;
ticks_t time_ticks_per_second(void) { return 1000u; }
ticks_t time_ticks(void) { return ++g_ticks; }
ticks_t time_ticks_per_second_hires(void) { return 1000u; }
ticks_t time_ticks_hires(void) { return g_ticks; }

void dprint(const UTF8_CHAR*, ...) {}

static unsigned long rand_next = 1;
void set_seed(unsigned long seed) { rand_next = seed; }
unsigned long pseudo_random(void) { /* the SunDog generator: noise tables and spectravoice phases depend on it */
    rand_next = (uint32_t)(rand_next * 1103515245u + 12345u);
    return ((unsigned int)(rand_next / 65536u) % 32768u);
}
int sundog_mutex_init(sundog_mutex* m, int) { m->locked = 0; return 0; }
int sundog_mutex_destroy(sundog_mutex*) { return 0; }
int sundog_mutex_lock(sundog_mutex* m) { m->locked = 1; return 0; }
int sundog_mutex_unlock(sundog_mutex* m) { m->locked = 0; return 0; }

sound_struct g_snd = {44100, 2};
void sound_stream_play(void) {}
void sound_stream_stop(void) {}

struct mem_file { const unsigned char* d; unsigned long size, pos; };
static const unsigned char* g_song;
static unsigned long g_song_size;
void sunvox_shim_set_song(const unsigned char* data, unsigned long size) { g_song = data; g_song_size = size; }
V3_FILE v3_open(const UTF8_CHAR*, const UTF8_CHAR* mode) {
    mem_file* f;
    if (g_song == NULL || mode[0] != 'r') return 0;
    f = (mem_file*)malloc(sizeof *f);
    if (f == NULL) return 0;
    f->d = g_song; f->size = g_song_size; f->pos = 0;
    return (V3_FILE)(uintptr_t)f;
}
int v3_close(V3_FILE f) { free((void*)(uintptr_t)f); return 0; }
int v3_seek(V3_FILE h, long off, long access) {
    mem_file* f = (mem_file*)(uintptr_t)h;
    long base = access == 1 ? (long)f->pos : access == 2 ? (long)f->size : 0;
    long np = base + off;
    if (np < 0) np = 0;
    if ((unsigned long)np > f->size) np = (long)f->size;
    f->pos = (unsigned long)np;
    return 0;
}
int v3_eof(V3_FILE h) { mem_file* f = (mem_file*)(uintptr_t)h; return f->pos >= f->size; }
ulong v3_read(void* ptr, ulong es, ulong n, V3_FILE h) {
    mem_file* f = (mem_file*)(uintptr_t)h;
    unsigned long want = (unsigned long)es * n, left = f->size - f->pos, got;
    if (es == 0) return 0;
    got = want < left ? want : left;
    memcpy(ptr, f->d + f->pos, got);
    f->pos += got;
    return (ulong)(got / es);
}
ulong v3_write(void*, ulong, ulong, V3_FILE) { return 0; }
void v3_putc(int, V3_FILE) {}
