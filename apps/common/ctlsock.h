/* Control socket (plan M1.2 phase 6, design doc section 8): a Unix domain
 * socket the shell listens on and `selecta-cli send` writes to. One protocol
 * line in, one reply line out, one connection per line. POSIX only; the
 * Amiga's equivalent is the ARexx port (M4). */
#ifndef SELECTA_CTLSOCK_H
#define SELECTA_CTLSOCK_H

#include <stddef.h>

/* $SELECTA_SOCKET, else $XDG_RUNTIME_DIR/selecta.sock, else
 * $TMPDIR/selecta-<uid>.sock (TMPDIR defaults to /tmp). 0 when it does not fit. */
int sel_ctl_path(char* out, size_t cap);

#define SEL_CTL_ALREADY_RUNNING (-2)

/* Creates and listens (mode 0600). A socket file nobody answers on is stale
 * and replaced. Returns the fd, SEL_CTL_ALREADY_RUNNING, or -1. */
int sel_ctl_listen(const char* path);

/* Fills `reply` for one received line; returns non-zero when the line counts
 * as an accepted command. */
typedef int (*sel_ctl_handler)(void* ctx, const char* line, char* reply, size_t cap);

/* Waits up to timeout_ms for one connection, reads one line, calls the
 * handler, writes the reply. Returns the number of accepted commands (0 or 1). */
int sel_ctl_serve(int fd, int timeout_ms, sel_ctl_handler handler, void* ctx);

void sel_ctl_close(int fd, const char* path);

/* Client: sends one line, stores the reply (no newline). 0 on success, -1 when
 * nothing listens on `path`. */
int sel_ctl_send(const char* path, const char* line, char* reply, size_t cap);

#endif /* SELECTA_CTLSOCK_H */
