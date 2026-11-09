/* POSIX terminal device (macOS and Linux), plan M1.1 phase 5 (row I59). */
#include "../term.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

const int txt_term_amiga_console = 0;
const char txt_term_enter_seq[] = "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h";
const char txt_term_leave_seq[] = "\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[0m\x1b[?1049l";

int txt_term_latin1(void) { return 0; }

/* Row I1169: anything the process writes to stderr lands on the screen the UI draws, at the cursor, and a line feed on
 * the last row scrolls the whole picture up one row. The grid's front copy no longer matches the screen then, so the
 * next diff flush blanks the cells where the picture was, not where it went: the start screen art stayed on the
 * player UI. While the UI owns the terminal, stderr that points at that same terminal goes to an unlinked file instead,
 * and txt_term_close writes it out after the screen is restored, so no message is lost. */
static void err_hold_begin(txt_term* t) {
    struct stat e, o;
    char path[64];
    const char* dir = getenv("TMPDIR");
    int fd, saved;
    if (fstat(2, &e) != 0 || fstat(t->out_fd, &o) != 0 || !isatty(2) || e.st_rdev != o.st_rdev) return;
    snprintf(path, sizeof path, "%s/selecta-stderr-XXXXXX", dir != NULL && strlen(dir) < 40u ? dir : "/tmp");
    fd = mkstemp(path);
    if (fd < 0) return;
    (void)unlink(path);
    fflush(stderr);
    saved = dup(2);
    if (saved < 0 || dup2(fd, 2) < 0) {
        if (saved >= 0) (void)close(saved);
        (void)close(fd);
        return;
    }
    t->err_saved = saved;
    t->err_hold = fd;
}

static void err_hold_end(txt_term* t) {
    char buf[1024];
    ssize_t n;
    if (t->err_saved < 0) return;
    fflush(stderr);
    (void)dup2(t->err_saved, 2);
    (void)close(t->err_saved);
    t->err_saved = -1;
    if (lseek(t->err_hold, 0, SEEK_SET) == 0) {
        while ((n = read(t->err_hold, buf, sizeof buf)) > 0) {
            if (write(2, buf, (size_t)n) < 0) break;
        }
    }
    (void)close(t->err_hold);
    t->err_hold = -1;
}

int txt_term_open(txt_term* t, int in_fd, int out_fd) {
    struct termios raw;
    memset(t, 0, sizeof *t);
    t->in_fd = in_fd;
    t->out_fd = out_fd;
    t->err_saved = -1;
    t->err_hold = -1;
    if (!isatty(in_fd)) return -1;
    if (sizeof(struct termios) > sizeof t->saved_bytes) return -1;
    if (tcgetattr(in_fd, (struct termios*)(void*)t->saved_bytes) != 0) return -1;
    raw = *(struct termios*)(void*)t->saved_bytes;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(in_fd, TCSANOW, &raw) != 0) return -1;
    t->active = 1;
    if (txt_term_write(t, txt_term_enter_seq, strlen(txt_term_enter_seq)) != 0) {
        txt_term_close(t);
        return -1;
    }
    err_hold_begin(t);
    return 0;
}

void txt_term_close(txt_term* t) {
    if (!t->active) return;
    t->active = 0;
    (void)txt_term_write(t, txt_term_leave_seq, strlen(txt_term_leave_seq));
    (void)tcsetattr(t->in_fd, TCSANOW, (struct termios*)(void*)t->saved_bytes);
    err_hold_end(t);
}

void txt_term_size(const txt_term* t, uint32_t* cols, uint32_t* rows) {
    struct winsize ws;
    *cols = 80;
    *rows = 24;
    if (ioctl(t->out_fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }
}

/* The size comes from ioctl on every call, so there is nothing to ask or to record. */
void txt_term_ask_size(txt_term* t) { (void)t; }

void txt_term_set_size(txt_term* t, uint32_t cols, uint32_t rows) {
    (void)t;
    (void)cols;
    (void)rows;
}

long txt_term_read(txt_term* t, uint8_t* buf, size_t cap, int timeout_ms) {
    struct pollfd p;
    int r;
    ssize_t n;
    p.fd = t->in_fd;
    p.events = POLLIN;
    p.revents = 0;
    do {
        r = poll(&p, 1, timeout_ms);
    } while (r < 0 && errno == EINTR);
    if (r < 0) return -1;
    if (r == 0) return 0;
    do {
        n = read(t->in_fd, buf, cap);
    } while (n < 0 && errno == EINTR);
    if (n < 0) return (errno == EAGAIN) ? 0 : -1;
    if (n == 0) return -1; /* end of input */
    return (long)n;
}

int txt_term_write(txt_term* t, const char* buf, size_t n) {
    while (n > 0) {
        ssize_t w = write(t->out_fd, buf, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) {
                struct pollfd p;
                p.fd = t->out_fd;
                p.events = POLLOUT;
                p.revents = 0;
                (void)poll(&p, 1, 100);
                continue;
            }
            return -1;
        }
        buf += w;
        n -= (size_t)w;
    }
    return 0;
}
