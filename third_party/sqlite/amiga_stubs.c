/* libnix lacks fchmod and fchown, which SQLite's unix VFS references from its syscall table. AmigaDOS has
 * no Unix owner or mode bits, so both are no-ops that report success. Project glue, not vendor source. */
int fchmod(int fd, unsigned int mode);
int fchown(int fd, unsigned int owner, unsigned int group);

int fchmod(int fd, unsigned int mode) {
    (void)fd;
    (void)mode;
    return 0;
}

int fchown(int fd, unsigned int owner, unsigned int group) {
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}
