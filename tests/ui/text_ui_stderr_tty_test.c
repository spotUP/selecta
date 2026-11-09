/* Row I1169 "the start screen art stays on the player UI": the real selecta binary runs the text UI on a pseudo
 * terminal, shows the I355 start screen, gets Space (which posts PLAY with no song, so the shell prints
 * "error no_song PLAY" on stderr) and then q. Before the fix that stderr line went to the same terminal while the UI
 * owned it; its line feed on the last row scrolled the picture up one row, and the next diff flush blanked the cells
 * where the art had been, so the art stayed on screen under the player UI. The test reads the terminal byte stream:
 * the error line must not appear while the UI owns the screen (before the leave sequence ESC[?1049l), and must appear
 * after it (no message lost). The error line itself is the sentinel that the stderr path ran.
 * Usage: text_ui_stderr_tty_test <selecta> <work dir> */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

static char g_buf[1 << 20];
static size_t g_len;

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static const char* find(const char* hay, size_t n, const char* needle) {
    size_t k = strlen(needle), i;
    for (i = 0; i + k <= n; i++) {
        if (memcmp(hay + i, needle, k) == 0) return hay + i;
    }
    return NULL;
}

/* Reads the pty until `until` shows up after byte `from`, the child closes it, or ms run out; 1 when found. */
static int pump(int fd, long ms, size_t from, const char* until) {
    long end = now_ms() + ms;
    while (now_ms() < end) {
        struct pollfd p;
        ssize_t n;
        p.fd = fd;
        p.events = POLLIN;
        p.revents = 0;
        if (poll(&p, 1, 50) <= 0) continue;
        n = read(fd, g_buf + g_len, sizeof g_buf - 1 - g_len);
        if (n <= 0) return 0; /* EOF or EIO: the child is gone */
        g_len += (size_t)n;
        g_buf[g_len] = '\0';
        if (until != NULL && g_len > from && find(g_buf + from, g_len - from, until) != NULL) return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    char cfg[1024], db[1024];
    struct winsize ws;
    int fd, st = 0, fails = 0;
    pid_t pid;
    const char *leave, *err;
    size_t mark;
    if (argc < 3) return 2;
    snprintf(cfg, sizeof cfg, "%s/stderr_tty_cfg", argv[2]);
    snprintf(db, sizeof db, "%s/stderr_tty_cfg/library.db", argv[2]);
    (void)mkdir(argv[2], 0755);
    (void)mkdir(cfg, 0755);
    (void)remove(db);
    memset(&ws, 0, sizeof ws);
    ws.ws_col = 100;
    ws.ws_row = 40;
    pid = forkpty(&fd, NULL, NULL, &ws);
    if (pid < 0) {
        perror("forkpty");
        return 2;
    }
    if (pid == 0) {
        execl(argv[1], argv[1], "--config-dir", cfg, "--db", db, "--output", "null", (char*)NULL);
        _exit(127);
    }
    /* the start screen's bottom row */
    if (!pump(fd, 15000, 0, "sPACE t0 sTART!")) {
        fprintf(stderr, "FAIL: the start screen never drew (%lu bytes)\n", (unsigned long)g_len);
        kill(pid, SIGKILL);
        return 1;
    }
    mark = g_len;
    if (write(fd, " ", 1) != 1) return 2;
    (void)pump(fd, 1500, mark, NULL);
    mark = g_len;
    if (write(fd, "q", 1) != 1) return 2;
    (void)pump(fd, 10000, mark, NULL);
    if (waitpid(pid, &st, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        (void)waitpid(pid, &st, 0);
    }
    close(fd);
    leave = find(g_buf, g_len, "\x1b[?1049l");
    err = find(g_buf, g_len, "error no_song");
    if (leave == NULL) {
        fprintf(stderr, "FAIL: the UI never left the alternate screen\n");
        fails++;
    }
    if (err == NULL) {
        fprintf(stderr, "FAIL: sentinel missing: no 'error no_song' on the terminal at all (Space did not reach PLAY)\n");
        fails++;
    } else if (leave != NULL && err < leave) {
        fprintf(stderr, "FAIL: stderr 'error no_song PLAY' reached the terminal at byte %ld while the UI owned the "
                        "screen (leave sequence at byte %ld): it scrolls the start screen art under the player UI\n",
                (long)(err - g_buf), (long)(leave - g_buf));
        fails++;
    }
    if (fails == 0) printf("[OK] I1169 stderr held while the text UI owns the terminal, written after it\n");
    return fails == 0 ? 0 : 1;
}
