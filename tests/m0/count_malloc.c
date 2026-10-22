/* no-alloc guard (plan Phase 5, allocation rule). Loaded into play-null
 * with DYLD_INSERT_LIBRARIES on macOS. Counts malloc, calloc and realloc
 * between the SEL_ALLOC_FREEZE and SEL_ALLOC_THAW markers the CLI writes to
 * stderr, then reports both counts on stderr through write(2):
 *   SEL_ALLOC_BEFORE=<calls before the freeze>   proves the shim counts
 *   SEL_ALLOC_COUNT=<calls inside the window>    must be 0
 * The window ends before the CLI prints its result line, because the
 * first printf allocates the stdout buffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void count(void) {
    if (g_frozen) {
        g_window++;
    } else {
        g_before++;
    }
}

static void* my_malloc(size_t n) {
    count();
    return malloc(n);
}

static void* my_calloc(size_t a, size_t b) {
    count();
    return calloc(a, b);
}

static void* my_realloc(void* p, size_t n) {
    count();
    return realloc(p, n);
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
DYLD_INTERPOSE(my_fputs, fputs);
DYLD_INTERPOSE(my_fwrite, fwrite);
