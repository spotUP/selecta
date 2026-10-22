/* POSIX directory iteration; built for macOS and Linux (see
 * src/library/CMakeLists.txt). Runs on the pump thread only. */
#include "../../library/dir.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int entry_is_dir(const char* dir, const struct dirent* de) {
#ifdef DT_DIR
    if (de->d_type == DT_DIR) {
        return 1;
    }
    if (de->d_type != DT_UNKNOWN && de->d_type != DT_LNK) {
        return 0;
    }
#endif
    {
        char full[PATH_MAX];
        struct stat st;
        if ((size_t)snprintf(full, sizeof full, "%s/%s", dir, de->d_name) >= sizeof full) {
            return 0;
        }
        return stat(full, &st) == 0 && S_ISDIR(st.st_mode);
    }
}

int sel_dir_each(const char* path, sel_dir_fn fn, void* ctx) {
    DIR* d = opendir(path);
    const struct dirent* de;
    if (d == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (!fn(ctx, de->d_name, entry_is_dir(path, de))) {
            break;
        }
    }
    closedir(d);
    return 1;
}
