/* Control socket, POSIX side. See ctlsock.h. */
#include "ctlsock.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define CTL_LINE_MAX 1024
#define CTL_READ_MS 500

int sel_ctl_path(char* out, size_t cap) {
    const char* over = getenv("SELECTA_SOCKET");
    const char* xdg = getenv("XDG_RUNTIME_DIR");
    const char* tmp = getenv("TMPDIR");
    int n;
    if (over != NULL && over[0] != '\0') {
        n = snprintf(out, cap, "%s", over);
    } else if (xdg != NULL && xdg[0] != '\0') {
        n = snprintf(out, cap, "%s/selecta.sock", xdg);
    } else if (tmp != NULL && tmp[0] != '\0') {
        n = snprintf(out, cap, "%s%sselecta-%lu.sock", tmp, tmp[strlen(tmp) - 1] == '/' ? "" : "/",
                     (unsigned long)getuid());
    } else {
        n = snprintf(out, cap, "/tmp/selecta-%lu.sock", (unsigned long)getuid());
    }
    return n > 0 && (size_t)n < cap;
}

static int fill_addr(struct sockaddr_un* a, const char* path) {
    memset(a, 0, sizeof *a);
    a->sun_family = AF_UNIX;
    if (strlen(path) >= sizeof a->sun_path) {
        return 0;
    }
    strcpy(a->sun_path, path);
    return 1;
}

static int connect_to(const char* path) {
    struct sockaddr_un a;
    int fd;
    if (!fill_addr(&a, path)) {
        return -1;
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&a, sizeof a) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int sel_ctl_listen(const char* path) {
    struct sockaddr_un a;
    mode_t old;
    int fd, probe;
    if (!fill_addr(&a, path)) {
        return -1;
    }
    probe = connect_to(path);
    if (probe >= 0) {
        close(probe);
        return SEL_CTL_ALREADY_RUNNING;
    }
    unlink(path); /* stale: the file exists but nobody answers */
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    old = umask(0177);
    if (bind(fd, (struct sockaddr*)&a, sizeof a) != 0 || listen(fd, 4) != 0) {
        umask(old);
        close(fd);
        return -1;
    }
    umask(old);
    signal(SIGPIPE, SIG_IGN);
    return fd;
}

static int read_line(int fd, char* buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        struct pollfd p;
        char c;
        p.fd = fd;
        p.events = POLLIN;
        if (poll(&p, 1, CTL_READ_MS) <= 0) {
            break;
        }
        if (read(fd, &c, 1) <= 0 || c == '\n') {
            break;
        }
        if (c != '\r') {
            buf[n++] = c;
        }
    }
    buf[n] = '\0';
    return n > 0;
}

int sel_ctl_serve(int fd, int timeout_ms, sel_ctl_handler handler, void* ctx) {
    struct pollfd p;
    char line[CTL_LINE_MAX], reply[CTL_LINE_MAX];
    int c, accepted = 0;
    p.fd = fd;
    p.events = POLLIN;
    if (poll(&p, 1, timeout_ms) <= 0 || !(p.revents & POLLIN)) {
        return 0;
    }
    c = accept(fd, NULL, NULL);
    if (c < 0) {
        return 0;
    }
    if (read_line(c, line, sizeof line)) {
        size_t n;
        reply[0] = '\0';
        accepted = handler(ctx, line, reply, sizeof reply - 1) ? 1 : 0;
        n = strlen(reply);
        reply[n] = '\n';
        (void)!write(c, reply, n + 1);
    }
    close(c);
    return accepted;
}

void sel_ctl_close(int fd, const char* path) {
    if (fd >= 0) {
        close(fd);
        unlink(path);
    }
}

int sel_ctl_send(const char* path, const char* line, char* reply, size_t cap) {
    int fd = connect_to(path);
    size_t n = 0;
    char msg[CTL_LINE_MAX + 2];
    if (fd < 0) {
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    snprintf(msg, sizeof msg, "%s\n", line);
    if (write(fd, msg, strlen(msg)) < 0) {
        close(fd);
        return -1;
    }
    while (n + 1 < cap) {
        char c;
        if (read(fd, &c, 1) <= 0 || c == '\n') {
            break;
        }
        reply[n++] = c;
    }
    reply[n] = '\0';
    close(fd);
    return 0;
}
