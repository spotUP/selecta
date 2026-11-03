/* POSIX backend of the SSDP socket shim (upnp_sock.h). */
#include "upnp/upnp_sock.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct sel_usock {
    int fd;
};

sel_usock* sel_usock_open(void) {
    sel_usock* s = (sel_usock*)malloc(sizeof *s);
    unsigned char ttl = 4;

    if (s == NULL) {
        return NULL;
    }
    s->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s->fd < 0) {
        free(s);
        return NULL;
    }
    (void)setsockopt(s->fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
    return s;
}

int sel_usock_send(sel_usock* s, const char* ip, unsigned port, const void* data, size_t len) {
    struct sockaddr_in to;

    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &to.sin_addr) != 1) {
        return 0;
    }
    return sendto(s->fd, data, len, 0, (struct sockaddr*)&to, sizeof to) == (ssize_t)len;
}

long sel_usock_recv(sel_usock* s, void* buf, size_t cap, unsigned timeout_ms, char* from) {
    struct pollfd pf;
    struct sockaddr_in src;
    socklen_t sl = sizeof src;
    ssize_t n;

    pf.fd = s->fd;
    pf.events = POLLIN;
    pf.revents = 0;
    if (poll(&pf, 1, (int)timeout_ms) <= 0) {
        return -1;
    }
    n = recvfrom(s->fd, buf, cap, 0, (struct sockaddr*)&src, &sl);
    if (n < 0) {
        return -1;
    }
    if (inet_ntop(AF_INET, &src.sin_addr, from, 16) == NULL) {
        from[0] = '\0';
    }
    return (long)n;
}

void sel_usock_close(sel_usock* s) {
    if (s != NULL) {
        close(s->fd);
        free(s);
    }
}
