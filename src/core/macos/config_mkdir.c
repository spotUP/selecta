/* Creates the config directory and any missing parents (POSIX; built for
 * macOS and Linux, see src/CMakeLists.txt). Declared in <selecta/engine.h>. */
#include <selecta/engine.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

int sel_config_mkdir(const char* dir) {
    char tmp[512];
    size_t n = strlen(dir), i;
    if (n == 0 || n >= sizeof tmp) {
        return 0;
    }
    memcpy(tmp, dir, n + 1u);
    for (i = 1; i <= n; i++) {
        if (tmp[i] == '/' || tmp[i] == '\0') {
            char keep = tmp[i];
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return 0;
            }
            tmp[i] = keep;
        }
    }
    return 1;
}
