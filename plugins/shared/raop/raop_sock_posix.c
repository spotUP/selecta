/* POSIX backend of raop_sock.h (macOS, Linux). Blocking sockets with poll() deadlines. */
#include "raop_sock.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int wait_fd(int fd, short ev, unsigned ms) {
    struct pollfd p;
    int r;
    p.fd = fd;
    p.events = ev;
    p.revents = 0;
    do r = poll(&p, 1, (int)ms);
    while (r < 0 && errno == EINTR);
    return r;
}

int raop_tcp_open(const char *host, unsigned port, unsigned timeout_ms) {
    struct addrinfo hints, *res, *ai;
    char pbuf[8];
    int fd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(pbuf, sizeof pbuf, "%u", port);
    if (getaddrinfo(host, pbuf, &hints, &res) != 0) return -1;
    for (ai = res; ai && fd < 0; ai = ai->ai_next) {
        int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        int fl, ok = 0;
        if (s < 0) continue;
        fl = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, fl | O_NONBLOCK);
        if (connect(s, ai->ai_addr, ai->ai_addrlen) == 0) {
            ok = 1;
        } else if (errno == EINPROGRESS && wait_fd(s, POLLOUT, timeout_ms) > 0) {
            int err = 0;
            socklen_t el = sizeof err;
            ok = getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el) == 0 && err == 0;
        }
        if (ok) {
            fcntl(s, F_SETFL, fl);
            fd = s;
        } else {
            close(s);
        }
    }
    freeaddrinfo(res);
    return fd;
}

int raop_tcp_send(int h, const void *data, size_t len, unsigned timeout_ms) {
    const char *p = data;
    while (len) {
        ssize_t n;
        if (wait_fd(h, POLLOUT, timeout_ms) <= 0) return -1;
#ifdef MSG_NOSIGNAL
        n = send(h, p, len, MSG_NOSIGNAL);
#else
        n = send(h, p, len, 0);
#endif
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

int raop_tcp_recv(int h, void *buf, size_t cap, unsigned timeout_ms) {
    ssize_t n;
    int r = wait_fd(h, POLLIN, timeout_ms);
    if (r == 0) return 0;
    if (r < 0) return -1;
    n = recv(h, buf, cap, 0);
    return n > 0 ? (int)n : -1;
}

int raop_tcp_local_ip(int h, char *out, size_t cap) {
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    if (getsockname(h, (struct sockaddr *)&sa, &sl) != 0 || sa.sin_family != AF_INET) return -1;
    return inet_ntop(AF_INET, &sa.sin_addr, out, (socklen_t)cap) ? 0 : -1;
}

void raop_tcp_close(int h) {
    if (h >= 0) close(h);
}

int raop_udp_open(void) { return socket(AF_INET, SOCK_DGRAM, 0); }

int raop_udp_send(int h, const char *host, unsigned port, const void *data, size_t len) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) return -1;
    return sendto(h, data, len, 0, (struct sockaddr *)&sa, sizeof sa) == (ssize_t)len ? 0 : -1;
}

void raop_udp_close(int h) {
    if (h >= 0) close(h);
}
