/* I1137: the directory of the running executable, the one rule behind sel_app_plugin_dir (src/core/config_dir.c) and
 * sel_app_data_dir (db.c). Lives in selecta_library because the library cannot call the core that links it. */
#include <selecta/engine.h>

#include <string.h>

#if !defined(__amigaos__) && !defined(AMIGA)
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#endif

int sel_app_exe_dir(char* out, size_t cap) {
    char exe[1024];
    char* slash;
#ifdef __APPLE__
    uint32_t sz = (uint32_t)sizeof exe;
    if (_NSGetExecutablePath(exe, &sz) != 0) {
        return 0;
    }
#else
    ssize_t r = readlink("/proc/self/exe", exe, sizeof exe - 1u);
    if (r <= 0) {
        return 0;
    }
    exe[r] = '\0';
#endif
    slash = strrchr(exe, '/');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';
    if (strlen(exe) >= cap) {
        return 0;
    }
    strcpy(out, exe);
    return 1;
}
#else
int sel_app_exe_dir(char* out, size_t cap) {
    (void)out;
    (void)cap;
    return 0;
}
#endif
