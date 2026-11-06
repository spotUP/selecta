/* I706c: re-save a SunVox song with the official SunVox Library (sv_load + sv_save), so the file carries the modern
 * version field that the 2009 BSD engine refuses. Built and run by sunvox_fixtures.sh; the library is dlopen'ed.
 * usage: sunvox_resave <sunvox library> <in.sunvox> <out.sunvox> */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    void *dl = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!dl) { fprintf(stderr, "dlopen %s failed\n", argv[1]); return 1; }
    int (*init)(const char *, int, int, unsigned) = dlsym(dl, "sv_init");
    int (*open_slot)(int) = dlsym(dl, "sv_open_slot");
    int (*load)(int, const char *) = dlsym(dl, "sv_load");
    int (*save)(int, const char *) = dlsym(dl, "sv_save");
    int (*close_slot)(int) = dlsym(dl, "sv_close_slot");
    int (*deinit)(void) = dlsym(dl, "sv_deinit");
    if (!init || !open_slot || !load || !save || !close_slot || !deinit) { fprintf(stderr, "missing symbol\n"); return 1; }
    /* flags: no debug output (1) | offline (2) | one thread (16) */
    if (init(NULL, 44100, 2, 1 | 2 | 16) < 0) { fprintf(stderr, "sv_init failed\n"); return 1; }
    int rc = 1;
    if (open_slot(0) == 0 && load(0, argv[2]) == 0 && save(0, argv[3]) == 0) rc = 0;
    else fprintf(stderr, "load/save failed\n");
    close_slot(0);
    deinit();
    return rc;
}
