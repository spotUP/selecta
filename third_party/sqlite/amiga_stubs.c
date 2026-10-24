#define _POSIX_C_SOURCE 200809L /* ftruncate, pread, pwrite in libnix unistd.h */
/* libnix lacks fchmod and fchown, which SQLite's unix VFS references from its syscall table. AmigaDOS has
 * no Unix owner or mode bits, so both are no-ops that report success. Project glue, not vendor source. */
#include <string.h>
#include <unistd.h>

int fchmod(int fd, unsigned int mode);

int fchmod(int fd, unsigned int mode) {
    (void)fd;
    (void)mode;
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

/* I225: libnix declares pread and pwrite but does not define them, so SQLite fell back to lseek + read/write.
 * libnix lseek past the end of a file grows the file with dos Write() from an uninitialised buffer, even
 * when the caller only wants to read: a new database got 24 junk bytes and every later open failed with
 * SQLITE_NOTADB. These never seek past the end. pread reads nothing there, as POSIX does; pwrite fills the gap
 * with zeros first. Neither keeps the file position (SQLite builds with HAVE_PREAD/HAVE_PWRITE only reads and
 * writes through these two). */
ssize_t pread(int fd, void* buf, size_t nbytes, off_t offset) {
    off_t size = lseek(fd, 0, SEEK_END);

    if (size < 0 || offset < 0) {
        return -1;
    }
    if (offset >= size) {
        return 0;
    }
    if (lseek(fd, offset, SEEK_SET) != offset) {
        return -1;
    }
    return read(fd, buf, nbytes);
}

ssize_t pwrite(int fd, const void* buf, size_t nbytes, off_t offset) {
    static const char zeros[512];
    off_t size = lseek(fd, 0, SEEK_END);

    if (size < 0 || offset < 0) {
        return -1;
    }
    while (size < offset) {
        size_t n = (size_t)(offset - size) < sizeof zeros ? (size_t)(offset - size) : sizeof zeros;
        ssize_t w = write(fd, zeros, n);

        if (w <= 0) {
            return -1;
        }
        size += w;
    }
    if (lseek(fd, offset, SEEK_SET) != offset) {
        return -1;
    }
    return write(fd, buf, nbytes);
}
