/* no-alloc guard (plan Phase 5, allocation rule). Loaded into play-null
 * with DYLD_INSERT_LIBRARIES on macOS. Counts malloc, calloc and realloc
 * between the SEL_ALLOC_FREEZE and SEL_ALLOC_THAW markers the CLI writes to
 * stderr, then reports both counts on stderr through write(2):
 *   SEL_ALLOC_BEFORE=<calls before the freeze>   proves the shim counts
 *   SEL_ALLOC_COUNT=<calls inside the window>    must be 0
 *   SEL_ALLOC_PEAK_BYTES=<most live heap bytes before the thaw>   (I1313,
 *     malloc_size of every live block; tools/player_ram.py takes a host
 *     player's RAM peak from it for players vamos cannot run; with
 *     SEL_ALLOC_PEAK_AT_EXIT set it also prints at process exit)
 * The window ends before the CLI prints its result line, because the
 * first printf allocates the stdout buffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc/malloc.h>
#include <unistd.h>

#define DYLD_INTERPOSE(replacement, replacee)                                                      \
    __attribute__((used)) static struct {                                                          \
        const void* replacement;                                                                   \
        const void* replacee;                                                                      \
    } _interpose_##replacee __attribute__((section("__DATA,__interpose"))) = {                     \
        (const void*)(unsigned long)&replacement, (const void*)(unsigned long)&replacee}

static volatile unsigned long g_before;
static volatile unsigned long g_window;
static volatile int g_frozen;
static size_t g_live;
static size_t g_peak;

static void grew(void* p) {
    if (p != NULL) {
        g_live += malloc_size(p);
        if (g_live > g_peak) {
            g_peak = g_live;
        }
    }
}

static void shrank(void* p) {
    if (p != NULL) {
        g_live -= malloc_size(p);
    }
}

static void count(void) {
    if (g_frozen) {
        g_window++;
    } else {
        g_before++;
    }
}

static void* my_malloc(size_t n) {
    void* p;
    count();
    p = malloc(n);
    grew(p);
    return p;
}

static void* my_calloc(size_t a, size_t b) {
    void* p;
    count();
    p = calloc(a, b);
    grew(p);
    return p;
}

static void* my_realloc(void* p, size_t n) {
    void* q;
    count();
    shrank(p);
    q = realloc(p, n);
    grew(q != NULL ? q : (n != 0 ? p : NULL));
    return q;
}

static void my_free(void* p) {
    shrank(p);
    free(p);
}

static void put_count(const char* label, unsigned long v) {
    char buf[64];
    char digits[24];
    size_t len = strlen(label), d = 0, i;
    memcpy(buf, label, len);
    do {
        digits[d++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    for (i = 0; i < d; i++) {
        buf[len++] = digits[d - 1 - i];
    }
    buf[len++] = '\n';
    write(2, buf, len);
}

/* The markers are caught at the stdio call the CLI makes; libc's own
 * write from inside fputs is bound within the shared cache and is not
 * interposed. */
static void marker(const void* s, size_t n) {
    if (n >= 16 && memcmp(s, "SEL_ALLOC_FREEZE", 16) == 0) {
        g_frozen = 1;
    } else if (n >= 14 && memcmp(s, "SEL_ALLOC_THAW", 14) == 0 && g_frozen) {
        g_frozen = 0;
        put_count("SEL_ALLOC_BEFORE=", g_before);
        put_count("SEL_ALLOC_COUNT=", g_window);
        put_count("SEL_ALLOC_PEAK_BYTES=", (unsigned long)g_peak);
    }
}

/* Runs without the markers (tools/player_ram.py render) report the peak at exit (I1313). */
__attribute__((destructor)) static void report_peak(void) {
    if (getenv("SEL_ALLOC_PEAK_AT_EXIT") != NULL) {
        put_count("SEL_ALLOC_PEAK_BYTES=", (unsigned long)g_peak);
    }
}

static int my_fputs(const char* s, FILE* f) {
    if (f == stderr) {
        marker(s, strlen(s));
    }
    return fputs(s, f);
}

static size_t my_fwrite(const void* p, size_t size, size_t n, FILE* f) {
    if (f == stderr) {
        marker(p, size * n);
    }
    return fwrite(p, size, n, f);
}

DYLD_INTERPOSE(my_malloc, malloc);
DYLD_INTERPOSE(my_calloc, calloc);
DYLD_INTERPOSE(my_realloc, realloc);
DYLD_INTERPOSE(my_free, free);
DYLD_INTERPOSE(my_fputs, fputs);
DYLD_INTERPOSE(my_fwrite, fwrite);
