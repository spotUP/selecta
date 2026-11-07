/* I685e2: the log gate of the MUI class package (design 2026-11-06-mui-mcc-package.md section 4, D5).
 * gate-off:  UP_MCC_DEBUG unset -> aa_log writes nothing.
 * gate-on:   UP_MCC_DEBUG=1 (set before the first call; the gate is read once) -> the line appears with the "up-mcc: " prefix.
 * source:    no fprintf(stderr outside mui_log.c in the package, no "selecta text inside an aa_log call. */
#include "map/mui_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail = 1; fprintf(stdout, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stdout, __VA_ARGS__); fputc('\n', stdout); } } while (0)

/* Runs aa_log with stderr redirected to a file and returns what it wrote. */
static size_t capture(const char* path, char* out, size_t cap) {
    FILE* f;
    size_t n;
    if (freopen(path, "w", stderr) == NULL) return (size_t)-1;
    aa_log("up-mcc: probe %d\n", 42);
    fflush(stderr);
    f = fopen(path, "rb");
    if (f == NULL) return (size_t)-1;
    n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    remove(path);
    return n;
}

static void test_gate(int on, const char* path) {
    char out[256];
    size_t n;
    if (on) setenv("UP_MCC_DEBUG", "1", 1); else unsetenv("UP_MCC_DEBUG");
    n = capture(path, out, sizeof out);
    if (on) CHECK(n != (size_t)-1 && strcmp(out, "up-mcc: probe 42\n") == 0, "gate on: got '%s'", out);
    else CHECK(n == 0, "gate off: wrote %lu bytes", (unsigned long)n);
}

static int read_file(const char* path, char* buf, size_t cap) {
    FILE* f = fopen(path, "rb");
    size_t n;
    if (f == NULL) return 0;
    n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
    return 1;
}

static void test_source(void) {
    static const char* const files[] = {"/src/mui_aa_draw.c", "/src/mui_aatip.c", "/src/map/mui_theme.c", "/src/class/mui_aafield_class.c", "/src/map/mui_log.c"};
    static char buf[300000];
    size_t i;
    int routed = 0;
    for (i = 0; i < sizeof files / sizeof files[0]; i++) {
        char path[1024];
        const char* p;
        snprintf(path, sizeof path, "%s%s", MCC_DIR, files[i]);
        CHECK(read_file(path, buf, sizeof buf), "cannot read %s", path);
        if (strstr(files[i], "mui_log.c") == NULL) CHECK(strstr(buf, "fprintf(stderr") == NULL, "%s still writes stderr directly", files[i]);
        if (strstr(files[i], "mui_log.c") != NULL) continue; /* the definition, not a call */
        for (p = buf; (p = strstr(p, "aa_log(")) != NULL; p++) {
            const char* e = strchr(p, ';');
            char call[1024];
            size_t len = e != NULL ? (size_t)(e - p) : 0;
            if (len == 0 || len >= sizeof call) continue;
            memcpy(call, p, len);
            call[len] = '\0';
            routed++;
            CHECK(strstr(call, "\"selecta") == NULL && strstr(call, "up-mcc: ") != NULL, "%s: aa_log text not prefixed up-mcc: %s", files[i], call);
        }
    }
    CHECK(routed >= 4, "only %d aa_log calls found in the four sites", routed);
}

int main(int argc, char** argv) {
    const char* t = argc > 1 ? argv[1] : "";
    const char* path = argc > 2 ? argv[2] : "mcc_log_capture.txt";
    if (!strcmp(t, "gate-off")) test_gate(0, path);
    else if (!strcmp(t, "gate-on")) test_gate(1, path);
    else if (!strcmp(t, "source")) test_source();
    else { fprintf(stdout, "unknown test '%s'\n", t); return 2; }
    if (g_fail) return 1;
    fprintf(stdout, "ok %s\n", t);
    return 0;
}
