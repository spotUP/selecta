/* libnix path calls with UTF-8 paths (I218d). libnix hands the path bytes to dos.library unchanged, but the engine
 * (and sqlite, curl-free third_party code, the plugins) speak UTF-8. The Amiga link wraps the libc functions below
 * (`-Wl,--wrap=<name>`, see src/CMakeLists.txt); each wrapper converts the path to Latin-1 and calls libnix's
 * `__real_<name>`. A path with no Latin-1 spelling fails with ENOENT: no such file can exist, and mapping it to '?'
 * would name a different file. ASCII paths, nearly all of them, call straight through. Calls inside libnix itself
 * are not wrapped, so nothing is converted twice. */
#include "dos_name.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

FILE* __real_fopen(const char* path, const char* mode);
int __real_open(const char* path, int flags, ...);
int __real_remove(const char* path);
int __real_rename(const char* from, const char* to);
int __real_unlink(const char* path);
int __real_mkdir(const char* path, mode_t mode);
int __real_rmdir(const char* path);
int __real_stat(const char* path, struct stat* st);
int __real_access(const char* path, int mode);
int __real_lstat(const char* path, struct stat* st);
int __real_utimes(const char* path, const struct timeval* tv);
ssize_t __real_readlink(const char* path, char* buf, size_t n);
char* __real_getcwd(char* buf, size_t n);

#define CONVERT(dos, path, fail)                              \
    do {                                                      \
        if (!sel_latin1_is_ascii(path)) {                     \
            if (!sel_dos_path(dos, sizeof dos, path)) {       \
                errno = ENOENT;                               \
                return fail;                                  \
            }                                                 \
            path = dos;                                       \
        }                                                     \
    } while (0)

FILE* __wrap_fopen(const char* path, const char* mode) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, NULL);
    if (path == dos) {
        /* libnix fopen() rejects a path holding a byte >= 0x80 before it reaches dos.library (measured under vamos:
         * no Open call, errno ENOENT) while open() accepts the same path, so a converted path goes open + fdopen. */
        int flags, fd;
        FILE* f;

        switch (mode[0]) {
        case 'r': flags = O_RDONLY; break;
        case 'w': flags = O_WRONLY | O_CREAT | O_TRUNC; break;
        case 'a': flags = O_WRONLY | O_CREAT | O_APPEND; break;
        default: errno = EINVAL; return NULL;
        }
        if (strchr(mode, '+') != NULL) {
            flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
        }
        fd = __real_open(path, flags, 0666);
        if (fd < 0) {
            return NULL;
        }
        f = fdopen(fd, mode);
        if (f == NULL) {
            close(fd);
        }
        return f;
    }
    return __real_fopen(path, mode);
}

int __wrap_open(const char* path, int flags, ...) {
    char dos[SEL_DOS_PATH_MAX];
    va_list ap;
    int mode;

    CONVERT(dos, path, -1);
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
    return __real_open(path, flags, mode);
}

int __wrap_remove(const char* path) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_remove(path);
}

int __wrap_rename(const char* from, const char* to) {
    char dos_from[SEL_DOS_PATH_MAX];
    char dos_to[SEL_DOS_PATH_MAX];

    CONVERT(dos_from, from, -1);
    CONVERT(dos_to, to, -1);
    return __real_rename(from, to);
}

int __wrap_unlink(const char* path) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_unlink(path);
}

int __wrap_mkdir(const char* path, mode_t mode) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_mkdir(path, mode);
}

int __wrap_rmdir(const char* path) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_rmdir(path);
}

int __wrap_stat(const char* path, struct stat* st) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_stat(path, st);
}

int __wrap_access(const char* path, int mode) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_access(path, mode);
}

int __wrap_lstat(const char* path, struct stat* st) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_lstat(path, st);
}

int __wrap_utimes(const char* path, const struct timeval* tv) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_utimes(path, tv);
}

/* The link target comes back as the volume stores it; only the path argument is converted. */
ssize_t __wrap_readlink(const char* path, char* buf, size_t n) {
    char dos[SEL_DOS_PATH_MAX];

    CONVERT(dos, path, -1);
    return __real_readlink(path, buf, n);
}

/* The one wrapped call whose result is a path: DOS returns Latin-1, the engine wants UTF-8. */
char* __wrap_getcwd(char* buf, size_t n) {
    char dos[SEL_DOS_PATH_MAX];

    if (buf == NULL || n == 0 || __real_getcwd(dos, sizeof dos) == NULL) {
        return NULL;
    }
    (void)sel_latin1_to_utf8(buf, n, dos, strlen(dos));
    return buf;
}
