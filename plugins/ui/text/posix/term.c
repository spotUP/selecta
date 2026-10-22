/* POSIX terminal device (macOS and Linux), plan M1.1 phase 5 (row I59). */
#include "../term.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

const char txt_term_enter_seq[] = "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h";
const char txt_term_leave_seq[] = "\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[0m\x1b[?1049l";

int txt_term_open(txt_term* t, int in_fd, int out_fd) {
    struct termios raw;
    memset(t, 0, sizeof *t);
    t->in_fd = in_fd;
    t->out_fd = out_fd;
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
    return 0;
}

void txt_term_close(txt_term* t) {
    if (!t->active) return;
    t->active = 0;
    (void)txt_term_write(t, txt_term_leave_seq, strlen(txt_term_leave_seq));
    (void)tcsetattr(t->in_fd, TCSANOW, (struct termios*)(void*)t->saved_bytes);
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
